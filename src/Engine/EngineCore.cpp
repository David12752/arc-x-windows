/*
 * ============================================================================
 * ARC System Audio Host - Headless Engine Core Implementation
 * ============================================================================
 */

#include "EngineCore.h"
#include "../Utils/Logger.h"
#include <variant>

namespace ARC
{

//=============================================================================
EngineCore::EngineCore()
    : vstChain_     (pluginHost_)
    , audioPipeline_(vstChain_, audioProcessor_, commandQueue_, metricsQueue_)
    , commandServer_(commandQueue_)
{
    Logger::log("EngineCore constructed");
}

EngineCore::~EngineCore()
{
    if (initialized_)
        shutdown();
}

//=============================================================================
juce::String EngineCore::initialize()
{
    Logger::log("EngineCore::initialize — starting subsystems");

    // -------------------------------------------------------------------------
    // 1. Config
    // -------------------------------------------------------------------------
    const auto configDir  = juce::File::getSpecialLocation(
                                juce::File::userApplicationDataDirectory)
                            .getChildFile("ARC");
    configDir.createDirectory();
    configManager_.initialize(configDir.getChildFile("engine-config.xml"));
    configManager_.load();

    // -------------------------------------------------------------------------
    // 2. Plugin host
    // -------------------------------------------------------------------------
    pluginHost_.initialize();

    // -------------------------------------------------------------------------
    // 3. WASAPI loopback capture
    // -------------------------------------------------------------------------
    if (!loopback_.initialize())
    {
        return "WASAPILoopback::initialize() failed — "
               "check that a default audio input device is present.";
    }

    const double sampleRate  = configManager_.getValue("sampleRate",  48000.0);
    const int    bufferSize  = configManager_.getValue("bufferSize",  256);

    loopback_.setProjectSampleRate(sampleRate);

    // Wire the loopback queue BEFORE start() so the first captured block
    // goes to the queue, not the callback.
    loopback_.setLoopbackQueue(&loopbackQueue_);
    audioPipeline_.setLoopbackQueue(&loopbackQueue_);

    if (!loopback_.start())
        return "WASAPILoopback::start() failed";

    // -------------------------------------------------------------------------
    // 4. Output device (AudioDeviceManager)
    // -------------------------------------------------------------------------
    const juce::String outputDevice = configManager_.getValue(
        "outputDevice", juce::String("default"));

    juce::String initError = audioDeviceManager_.initialize(
        "",            // no direct input (we use WASAPI loopback)
        outputDevice,
        sampleRate,
        bufferSize);

    if (initError.isNotEmpty())
    {
        Logger::warning("EngineCore: AudioDeviceManager init warning: " + initError);
        // Attempt fallback with default device
        initError = audioDeviceManager_.initialize("", "", sampleRate, bufferSize);
        if (initError.isNotEmpty())
            return "AudioDeviceManager::initialize() failed: " + initError;
    }

    // -------------------------------------------------------------------------
    // 5. Wire audio pipeline as the callback
    // -------------------------------------------------------------------------
    audioDeviceManager_.setAudioCallback(&audioPipeline_);

    // -------------------------------------------------------------------------
    // 5b. VB-Cable detection (Non-RT, one-shot on startup)
    // -------------------------------------------------------------------------
    lastVBCableInfo_ = VBCableWrapper::detectVBCable(audioDeviceManager_);
    if (lastVBCableInfo_.isFullyAvailable())
        Logger::log("EngineCore: VB-Audio Virtual Cable detected");
    else
        Logger::log("EngineCore: VB-Audio Virtual Cable not present");

    // -------------------------------------------------------------------------
    // 6. Command server (starts background pipe thread)
    // -------------------------------------------------------------------------
    commandServer_.setConnectCallback([this]
    {
        Logger::log("EngineCore: UI client connected");
        // Push a status event so the UI can sync immediately
        commandServer_.pushEvent(R"({"event":"connected","version":"2.0.0"})");
        // Also broadcast VB-Cable status so the UI shows the right badge
        broadcastVBCableStatus(lastVBCableInfo_);
    });

    commandServer_.setDisconnectCallback([this]
    {
        Logger::log("EngineCore: UI client disconnected");
    });

    if (!commandServer_.start())
        Logger::warning("EngineCore: CommandServer failed to start (UI integration disabled)");

    // -------------------------------------------------------------------------
    // 7. Shared memory metrics publisher
    // -------------------------------------------------------------------------
    if (!metricsPublisher_.open())
        Logger::warning("EngineCore: MetricsPublisher failed — UI will not receive metrics");

    // -------------------------------------------------------------------------
    // 8. 60Hz metrics timer
    // -------------------------------------------------------------------------
    startTimer(16); // ~62Hz

    initialized_ = true;
    Logger::log("EngineCore::initialize — all subsystems started");
    return {};
}

//=============================================================================
void EngineCore::shutdown()
{
    Logger::log("EngineCore::shutdown — stopping subsystems");

    // 1. Timer first (stops metrics pump before we tear down the queues)
    stopTimer();

    // 2. Command server
    commandServer_.stop();

    // 3. Audio callback + device — stop RT processing first
    audioDeviceManager_.removeAudioCallback();
    audioDeviceManager_.shutdown();

    // 4. WASAPI capture — detach queue before stopping thread
    loopback_.setLoopbackQueue(nullptr);
    audioPipeline_.setLoopbackQueue(nullptr);
    loopback_.stop();

    // 5. Plugin host
    pluginHost_.shutdown();

    // 6. Shared memory
    metricsPublisher_.close();

    // 7. Persist settings
    configManager_.save();

    initialized_ = false;
    Logger::log("EngineCore::shutdown — complete");
}

//=============================================================================
void EngineCore::hiResTimerCallback()
{
    // ------------------------------------------------------------------
    // Drain the metrics SPSC queue written by the RT callback.
    // Publish the most recent frame to shared memory.
    // ------------------------------------------------------------------
    MetricsFrame latest{};
    bool         gotFrame = false;

    MetricsFrame frame;
    while (metricsQueue_.try_dequeue(frame))
    {
        latest   = frame;
        gotFrame = true;
    }

    if (gotFrame)
    {
        // Augment with non-RT data the RT callback can't safely compute
        latest.cpuPeakPercent   = latest.cpuPercent; // TODO: sliding max
        latest.activePluginCount = static_cast<int32_t>(vstChain_.getActivePluginCount());
        latest.srcActive         = loopback_.isSRCActive() ? 1 : 0;

        metricsPublisher_.publish(latest);
    }

    // ------------------------------------------------------------------
    // Drain any Non-RT commands that bypassed the RT pipeline
    // (e.g. LoadPlugin, ScanPlugins, SetSampleRate)
    // ------------------------------------------------------------------
    // NOTE: We check for a "shadow" command queue here.  In practice,
    // EngineCore inspects inbound commands BEFORE they're enqueued to the
    // RT queue so LoadPlugin goes to the plugin host, not the RT thread.
    //
    // This is implemented via CommandServer callback interception (Phase 2).
    // For Phase 1 the RT queue drains everything.
}

//=============================================================================
void EngineCore::broadcastVBCableStatus(const VBCableInfo& info)
{
    const juce::String installed  = info.isFullyAvailable() ? "true" : "false";
    const juce::String renderEsc  = info.renderDeviceName.replace("\"", "\\\"");
    const juce::String captureEsc = info.captureDeviceName.replace("\"", "\\\"");

    const juce::String json =
        "{\"event\":\"vbcable_status\","
        "\"installed\":" + installed + ","
        "\"renderDevice\":\"" + renderEsc + "\","
        "\"captureDevice\":\"" + captureEsc + "\"}";

    commandServer_.pushEvent(json);
}

//=============================================================================
// Non-RT command handlers for VB-Cable operations
void EngineCore::handleNonRTCommand(const EngineCommand& cmd)
{
    std::visit([this](auto&& c)
    {
        using T = std::decay_t<decltype(c)>;

        if constexpr (std::is_same_v<T, DetectVBCableCmd>)
        {
            Logger::log("EngineCore: DetectVBCableCmd — re-scanning device list");
            lastVBCableInfo_ = VBCableWrapper::detectVBCable(audioDeviceManager_);
            broadcastVBCableStatus(lastVBCableInfo_);
        }
        else if constexpr (std::is_same_v<T, SetVBCableRoutingCmd>)
        {
            const double sampleRate = configManager_.getValue("sampleRate", 48000.0);
            const int    bufferSize = configManager_.getValue("bufferSize",  256);

            if (c.enable != 0)
            {
                Logger::log("EngineCore: SetVBCableRoutingCmd — applying VB-Cable routing");
                const juce::String err = VBCableWrapper::applyRouting(
                    audioDeviceManager_, sampleRate, bufferSize);
                if (err.isEmpty())
                {
                    configManager_.setValue("useVBCableRouting", 1);
                    commandServer_.pushEvent(R"({"event":"routing_applied","device":"vbcable"})");
                }
                else
                {
                    const juce::String msg = "{\"event\":\"error\","
                        "\"message\":\"" + err.replace("\"", "\\\"") + "\"}";
                    commandServer_.pushEvent(msg);
                }
            }
            else
            {
                Logger::log("EngineCore: SetVBCableRoutingCmd — reverting to default");
                const juce::String err = VBCableWrapper::revertToDefault(
                    audioDeviceManager_, sampleRate, bufferSize);
                if (err.isEmpty())
                {
                    configManager_.setValue("useVBCableRouting", 0);
                    commandServer_.pushEvent(R"({"event":"routing_applied","device":"default"})");
                }
                else
                {
                    const juce::String msg = "{\"event\":\"error\","
                        "\"message\":\"" + err.replace("\"", "\\\"") + "\"}";
                    commandServer_.pushEvent(msg);
                }
            }
        }
        // All other non-RT commands handled elsewhere
    }, cmd);
}

} // namespace ARC
