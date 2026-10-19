/*
 * ============================================================================
 * ARC System Audio Host - Headless Engine Core
 *
 * EngineCore owns and coordinates all engine subsystems:
 *
 *   WASAPILoopback   — system audio capture
 *   AudioDeviceManager — output device + callback dispatch
 *   PluginHost       — VST3 format manager + plugin discovery
 *   VSTChain         — serial plugin chain
 *   AudioProcessor   — post-chain DSP (limiter, I/O gain)
 *   AudioPipeline    — RT callback (wraps chain + processor)
 *   CommandServer    — Named Pipe server, inbound commands from UI
 *   MetricsPublisher — Shared Memory seqlock writer, outbound metrics to UI
 *
 * Threading
 * ----------
 *   Main thread   — start()/stop(), non-critical setup
 *   RT thread     — AudioPipeline::processBlock (zero heap, zero locks)
 *   Pipe thread   — CommandServer (one background thread, blocks on pipe I/O)
 *   Timer thread  — metricsTimer_ (60Hz, JUCE HighResolutionTimer, Non-RT)
 *
 * Startup sequence (initialize())
 * --------------------------------
 *   1. ConfigManager.load()
 *   2. PluginHost.initialize()
 *   3. WASAPILoopback.initialize() + setProjectSampleRate()
 *   4. AudioDeviceManager.initialize(outputDevice, sampleRate, bufferSize)
 *   5. AudioDeviceManager.setAudioCallback(&audioPipeline_)
 *   6. CommandServer.start()
 *   7. MetricsPublisher.open()
 *   8. metricsTimer_.startTimer(16)   // ~60Hz
 *
 * Shutdown sequence (shutdown())
 * --------------------------------
 *   1. metricsTimer_.stopTimer()
 *   2. CommandServer.stop()
 *   3. AudioDeviceManager.removeAudioCallback() + shutdown()
 *   4. WASAPILoopback.stop()
 *   5. PluginHost.shutdown()
 *   6. MetricsPublisher.close()
 *   7. ConfigManager.save()
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include "../AudioEngine/AudioDeviceManager.h"
#include "../AudioEngine/AudioProcessor.h"
#include "../AudioEngine/PluginHost.h"
#include "../AudioEngine/WASAPILoopback.h"
#include "../AudioEngine/VBCableWrapper.h"
#include "../VST/VSTChain.h"
#include "../IPC/CommandServer.h"
#include "../IPC/MetricsPublisher.h"
#include "../Concurrency/SPSCQueue.h"
#include "../Concurrency/MetricsFrame.h"
#include "../Utils/ConfigManager.h"
#include "AudioPipeline.h"
#include <memory>
#include <atomic>

namespace ARC
{

//=============================================================================
/**
 * EngineCore — the root object of the headless `arc-engine.exe` process.
 *
 * Owned by ARCEngineApp (engine_main.cpp).  All subsystems are
 * stack-allocated members (no dynamic allocation after construction).
 */
class EngineCore : private juce::HighResolutionTimer
{
public:
    //=========================================================================
    EngineCore();
    ~EngineCore() override;

    EngineCore(const EngineCore&)            = delete;
    EngineCore& operator=(const EngineCore&) = delete;

    //=========================================================================
    /**
     * Initialize and start all subsystems.
     * @return Empty string on success; error description on failure.
     */
    juce::String initialize();

    /** Stop all subsystems and release resources. */
    void shutdown();

    bool isRunning() const noexcept { return initialized_; }

    //=========================================================================
    // Accessors for diagnostics / future RPC extensions
    AudioDeviceManager& getAudioDeviceManager() { return audioDeviceManager_; }
    PluginHost&         getPluginHost()          { return pluginHost_; }
    VSTChain&           getVSTChain()            { return vstChain_; }
    CommandServer&      getCommandServer()       { return commandServer_; }
    VBCableWrapper&     getVBCableWrapper()      { return vbCable_; }

private:
    //=========================================================================
    // HighResolutionTimer callback — runs at 60Hz on a Non-RT timer thread.
    // Drains metricsQueue_ and publishes to shared memory.
    void hiResTimerCallback() override;

    // Non-RT command handling for commands that require heap/disk (e.g. LoadPlugin)
    void handleNonRTCommand(const EngineCommand& cmd);

    //=========================================================================
    // SPSC queues — shared between RT and Non-RT contexts
    using CommandQueue  = SPSCQueue<EngineCommand, 256>;
    using MetricsQueue  = SPSCQueue<MetricsFrame,  64>;
    using LoopbackQueue = SPSCQueue<AudioBlock,     16>;

    CommandQueue  commandQueue_;
    MetricsQueue  metricsQueue_;
    LoopbackQueue loopbackQueue_;  // WASAPILoopback → AudioPipeline (RT-safe)

    //=========================================================================
    // Subsystems (construction order = dependency order)
    ConfigManager        configManager_;
    PluginHost           pluginHost_;
    AudioDeviceManager   audioDeviceManager_;
    AudioProcessor       audioProcessor_;
    WASAPILoopback       loopback_;
    VSTChain             vstChain_;
    AudioPipeline        audioPipeline_;
    CommandServer        commandServer_;
    MetricsPublisher     metricsPublisher_;
    VBCableWrapper       vbCable_;            // stateless helper

    //=========================================================================
    bool  initialized_{false};

    // Cached VB-Cable detection result — refreshed on initialize() and on
    // DetectVBCableCmd.
    VBCableInfo  lastVBCableInfo_;

    // Broadcast latest VB-Cable status over the events pipe.
    void broadcastVBCableStatus(const VBCableInfo& info);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EngineCore)
};

} // namespace ARC
