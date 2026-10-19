/*
 * ============================================================================
 * ARC System Audio Host - Audio Device Manager Implementation
 * ============================================================================
 */

#include "AudioDeviceManager.h"
#include "../Utils/Logger.h"
#include "../Utils/Constants.h"
#include "../Utils/RTGuard.h"
#include <cmath>
#include <exception>

namespace ARC
{

//=============================================================================
AudioDeviceManager::AudioDeviceManager()
{
    Logger::log("AudioDeviceManager created");
}

AudioDeviceManager::~AudioDeviceManager()
{
    shutdown();
    Logger::log("AudioDeviceManager destroyed");
}

//=============================================================================
juce::String AudioDeviceManager::initialize(const juce::String& inputDevice,
                                            const juce::String& outputDevice,
                                            double sampleRate,
                                            int bufferSize)
{
    Logger::log("Initializing audio device manager...");

    // Set up the device manager
    deviceManager.addChangeListener(this);

    // Try to initialize with WASAPI first (preferred for low latency)
    auto* deviceType = deviceManager.getAvailableDeviceTypes()[0];
    if (deviceType != nullptr)
    {
        deviceManager.setCurrentAudioDeviceType(deviceType->getTypeName(), true);
    }

    // Configure audio device setup
    currentSetup.inputDeviceName = inputDevice;
    currentSetup.outputDeviceName = outputDevice;
    currentSetup.sampleRate = sampleRate;
    currentSetup.bufferSize = bufferSize;
    currentSetup.inputChannels.setRange(0, 2, true);
    currentSetup.outputChannels.setRange(0, 2, true);
    currentSetup.useDefaultInputChannels = true;
    currentSetup.useDefaultOutputChannels = true;

    // Initialize the device
    juce::String error = deviceManager.setAudioDeviceSetup(currentSetup, true);

    if (error.isEmpty())
    {
        // Add ourselves as the audio callback
        deviceManager.addAudioCallback(this);

        Logger::log("Audio device initialized successfully");
        Logger::log("  Output device: " + getCurrentOutputDevice());
        Logger::log("  Sample rate: " + juce::String(getCurrentSampleRate()));
        Logger::log("  Buffer size: " + juce::String(getCurrentBufferSize()));
    }
    else
    {
        Logger::log("Audio device initialization failed: " + error);
    }

    return error;
}

void AudioDeviceManager::shutdown()
{
    Logger::log("Shutting down audio device manager...");

    deviceManager.removeAudioCallback(this);
    deviceManager.closeAudioDevice();
    deviceManager.removeChangeListener(this);
}

//=============================================================================
std::vector<AudioDeviceInfo> AudioDeviceManager::getAvailableInputDevices() const
{
    std::vector<AudioDeviceInfo> devices;

    auto* currentType = deviceManager.getCurrentDeviceTypeObject();
    if (currentType != nullptr)
    {
        auto deviceNames = currentType->getDeviceNames(true);
        for (const auto& name : deviceNames)
        {
            AudioDeviceInfo info;
            info.name = name;
            info.typeName = currentType->getTypeName();
            info.isDefault = false;
            devices.push_back(info);
        }
    }

    return devices;
}

std::vector<AudioDeviceInfo> AudioDeviceManager::getAvailableOutputDevices() const
{
    std::vector<AudioDeviceInfo> devices;

    auto* currentType = deviceManager.getCurrentDeviceTypeObject();
    if (currentType != nullptr)
    {
        auto deviceNames = currentType->getDeviceNames(false);
        for (const auto& name : deviceNames)
        {
            AudioDeviceInfo info;
            info.name = name;
            info.typeName = currentType->getTypeName();
            info.isDefault = false;
            devices.push_back(info);
        }
    }

    return devices;
}

juce::StringArray AudioDeviceManager::getDeviceTypeNames() const
{
    juce::StringArray names;
    // Note: getAvailableDeviceTypes() is not const in JUCE 7.x
    // Return empty for now
    return names;
}

//=============================================================================
juce::String AudioDeviceManager::setInputDevice(const juce::String& deviceName)
{
    currentSetup.inputDeviceName = deviceName;
    return deviceManager.setAudioDeviceSetup(currentSetup, true);
}

juce::String AudioDeviceManager::setOutputDevice(const juce::String& deviceName)
{
    currentSetup.outputDeviceName = deviceName;
    return deviceManager.setAudioDeviceSetup(currentSetup, true);
}

juce::String AudioDeviceManager::setSampleRate(double sampleRate)
{
    currentSetup.sampleRate = sampleRate;
    return deviceManager.setAudioDeviceSetup(currentSetup, true);
}

juce::String AudioDeviceManager::setBufferSize(int bufferSize)
{
    currentSetup.bufferSize = bufferSize;
    return deviceManager.setAudioDeviceSetup(currentSetup, true);
}

//=============================================================================
juce::String AudioDeviceManager::getCurrentInputDevice() const
{
    return currentSetup.inputDeviceName;
}

juce::String AudioDeviceManager::getCurrentOutputDevice() const
{
    return currentSetup.outputDeviceName;
}

double AudioDeviceManager::getCurrentSampleRate() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getCurrentSampleRate();
    return currentSetup.sampleRate;
}

int AudioDeviceManager::getCurrentBufferSize() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getCurrentBufferSizeSamples();
    return currentSetup.bufferSize;
}

//=============================================================================
void AudioDeviceManager::setAudioCallback(AudioCallback* callback)
{
    std::lock_guard<std::mutex> lock(callbackMutex);

    // Prepare BEFORE making visible to RT thread
    if (callback != nullptr)
        callback->prepareToPlay(getCurrentSampleRate(), getCurrentBufferSize());

    audioCallback.store(callback, std::memory_order_release);
}

void AudioDeviceManager::removeAudioCallback()
{
    std::lock_guard<std::mutex> lock(callbackMutex);

    // Atomically swap out, then release resources outside the RT path
    AudioCallback* old = audioCallback.exchange(nullptr, std::memory_order_acq_rel);
    if (old != nullptr)
        old->releaseResources();
}

//=============================================================================
void AudioDeviceManager::setMasterVolume(float volume)
{
    masterVolume.store(juce::jlimit(0.0f, 2.0f, volume));
}

float AudioDeviceManager::getMasterVolume() const
{
    return masterVolume.load();
}

void AudioDeviceManager::setMasterBypass(bool bypass)
{
    masterBypass.store(bypass);
}

bool AudioDeviceManager::isMasterBypassed() const
{
    return masterBypass.load();
}

//=============================================================================
AudioStats AudioDeviceManager::getAudioStats() const
{
    std::lock_guard<std::mutex> lock(statsMutex);

    AudioStats currentStats = stats;
    currentStats.peakLevelL = peakLevelL.load();
    currentStats.peakLevelR = peakLevelR.load();
    currentStats.rmsLevelL = rmsLevelL.load();
    currentStats.rmsLevelR = rmsLevelR.load();
    currentStats.xrunCount = xrunCount.load();
    currentStats.currentSampleRate = getCurrentSampleRate();
    currentStats.currentBufferSize = getCurrentBufferSize();

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        currentStats.inputLatencyMs = device->getInputLatencyInSamples() *
            1000.0 / currentStats.currentSampleRate;
        currentStats.outputLatencyMs = device->getOutputLatencyInSamples() *
            1000.0 / currentStats.currentSampleRate;
        currentStats.totalLatencyMs = currentStats.inputLatencyMs +
            currentStats.outputLatencyMs +
            (currentStats.currentBufferSize * 1000.0 / currentStats.currentSampleRate);
    }

    return currentStats;
}

void AudioDeviceManager::resetXrunCount()
{
    xrunCount.store(0);
}

//=============================================================================
void AudioDeviceManager::setLimiterEnabled(bool enabled)
{
    limiterEnabled.store(enabled);
}

bool AudioDeviceManager::isLimiterEnabled() const
{
    return limiterEnabled.load();
}

void AudioDeviceManager::setLimiterThreshold(float thresholdDb)
{
    limiterThreshold.store(thresholdDb);
}

//=============================================================================
void AudioDeviceManager::addDeviceChangeCallback(DeviceChangeCallback callback)
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    deviceChangeCallbacks.push_back(callback);
}

//=============================================================================
void AudioDeviceManager::audioDeviceIOCallbackWithContext(
    const float* const* inputChannelData,
    int numInputChannels,
    float* const* outputChannelData,
    int numOutputChannels,
    int numSamples,
    const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused(context);

    // Mark this thread as the RT audio callback thread on first entry.
    // thread_local write is free after the first call (flag already set).
    RTGuard::markCurrentThreadAsAudioCallback();

    auto startTime = juce::Time::getHighResolutionTicks();

    // processingBuffer is pre-allocated in audioDeviceAboutToStart.
    // If sizes mismatch here (device changed mid-stream), output silence and
    // increment xrun — never allocate heap from the RT thread.
    if (processingBuffer.getNumChannels() < numOutputChannels ||
        processingBuffer.getNumSamples() < numSamples)
    {
        for (int ch = 0; ch < numOutputChannels; ++ch)
            if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
        xrunCount.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    // Copy input to processing buffer
    for (int ch = 0; ch < juce::jmin(numInputChannels, numOutputChannels); ++ch)
    {
        if (inputChannelData[ch] != nullptr)
        {
            processingBuffer.copyFrom(ch, 0, inputChannelData[ch], numSamples);
        }
        else
        {
            processingBuffer.clear(ch, 0, numSamples);
        }
    }

    // Clear any extra channels
    for (int ch = numInputChannels; ch < numOutputChannels; ++ch)
    {
        processingBuffer.clear(ch, 0, numSamples);
    }

    // ---- RT-SAFE processing section ----------------------------------------
    // NO mutex, NO heap allocation, NO I/O below this line.
    // All cross-thread state is accessed via std::atomic load/store only.
    // -------------------------------------------------------------------------
    if (!masterBypass.load(std::memory_order_relaxed))
    {
        // Single acquire load — RT thread reads the prepared callback pointer.
        AudioCallback* cb = audioCallback.load(std::memory_order_acquire);
        if (cb != nullptr)
        {
            midiBuffer.clear();
            cb->processBlock(processingBuffer, midiBuffer);
        }
    }

    // Apply master volume
    const float volume = masterVolume.load(std::memory_order_relaxed);
    if (std::abs(volume - 1.0f) > 0.0001f)
        processingBuffer.applyGain(volume);

    // Apply limiter if enabled
    if (limiterEnabled.load(std::memory_order_relaxed))
        applyLimiter(processingBuffer);

    // Update metering atomics (no mutex — individual atomic stores)
    updateAudioStats(numSamples);

    // Copy to output
    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        if (outputChannelData[ch] != nullptr)
        {
            juce::FloatVectorOperations::copy(outputChannelData[ch],
                processingBuffer.getReadPointer(ch), numSamples);
        }
    }

    // Calculate CPU usage
    auto endTime = juce::Time::getHighResolutionTicks();
    double callbackDuration = juce::Time::highResolutionTicksToSeconds(endTime - startTime);
    double bufferDuration = numSamples / getCurrentSampleRate();
    double currentCpu = (callbackDuration / bufferDuration) * 100.0;

    cpuUsageSmoothed = cpuUsageSmoothed * 0.95 + currentCpu * 0.05;

    {
        std::lock_guard<std::mutex> lock(statsMutex);
        stats.cpuUsage = cpuUsageSmoothed;
    }
}

void AudioDeviceManager::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    Logger::log("Audio device starting: " + device->getName());

    const double sampleRate = device->getCurrentSampleRate();
    const int    bufferSize  = device->getCurrentBufferSizeSamples();

    // Pre-allocate with 2x headroom — the RT callback must NEVER resize.
    processingBuffer.setSize(2, juce::nextPowerOfTwo(bufferSize * 2),
                             false, false, true);
    processingBuffer.clear();

    std::lock_guard<std::mutex> lock(callbackMutex);
    AudioCallback* cb = audioCallback.load(std::memory_order_acquire);
    if (cb != nullptr)
        cb->prepareToPlay(sampleRate, bufferSize);
}

void AudioDeviceManager::audioDeviceStopped()
{
    Logger::log("Audio device stopped");

    std::lock_guard<std::mutex> lock(callbackMutex);
    AudioCallback* cb = audioCallback.load(std::memory_order_acquire);
    if (cb != nullptr)
        cb->releaseResources();
}

void AudioDeviceManager::audioDeviceError(const juce::String& errorMessage)
{
    Logger::log("Audio device error: " + errorMessage);
    xrunCount.fetch_add(1);
}

//=============================================================================
void AudioDeviceManager::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    juce::ignoreUnused(source);

    Logger::log("Audio device configuration changed");

    // Notify registered callbacks
    std::lock_guard<std::mutex> lock(callbackMutex);
    for (auto& callback : deviceChangeCallbacks)
    {
        callback();
    }
}

//=============================================================================
void AudioDeviceManager::updateAudioStats(int numSamples)
{
    juce::ignoreUnused(numSamples);

    // Calculate peak and RMS levels
    if (processingBuffer.getNumChannels() >= 2)
    {
        float newPeakL = calculatePeakLevel(processingBuffer.getReadPointer(0),
                                            processingBuffer.getNumSamples());
        float newPeakR = calculatePeakLevel(processingBuffer.getReadPointer(1),
                                            processingBuffer.getNumSamples());

        // Peak with decay
        float currentPeakL = peakLevelL.load();
        float currentPeakR = peakLevelR.load();

        peakLevelL.store(juce::jmax(newPeakL, currentPeakL * peakDecay));
        peakLevelR.store(juce::jmax(newPeakR, currentPeakR * peakDecay));

        // RMS levels
        float newRmsL = calculateRMSLevel(processingBuffer.getReadPointer(0),
                                          processingBuffer.getNumSamples());
        float newRmsR = calculateRMSLevel(processingBuffer.getReadPointer(1),
                                          processingBuffer.getNumSamples());

        float currentRmsL = rmsLevelL.load();
        float currentRmsR = rmsLevelR.load();

        rmsLevelL.store(currentRmsL * 0.9f + newRmsL * 0.1f);
        rmsLevelR.store(currentRmsR * 0.9f + newRmsR * 0.1f);
    }
}

void AudioDeviceManager::applyLimiter(juce::AudioBuffer<float>& buffer)
{
    float threshold = juce::Decibels::decibelsToGain(limiterThreshold.load());
    float attackCoeff = std::exp(-1.0f / (LIMITER_ATTACK * (float)getCurrentSampleRate()));
    float releaseCoeff = std::exp(-1.0f / (LIMITER_RELEASE * (float)getCurrentSampleRate()));

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float maxSample = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            maxSample = juce::jmax(maxSample, std::abs(buffer.getSample(ch, sample)));
        }

        float targetGain = maxSample > threshold ? threshold / maxSample : 1.0f;

        if (targetGain < limiterGain)
        {
            limiterGain = attackCoeff * limiterGain + (1.0f - attackCoeff) * targetGain;
        }
        else
        {
            limiterGain = releaseCoeff * limiterGain + (1.0f - releaseCoeff) * targetGain;
        }

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            buffer.setSample(ch, sample, buffer.getSample(ch, sample) * limiterGain);
        }
    }
}

float AudioDeviceManager::calculatePeakLevel(const float* data, int numSamples)
{
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        peak = juce::jmax(peak, std::abs(data[i]));
    }
    return peak;
}

float AudioDeviceManager::calculateRMSLevel(const float* data, int numSamples)
{
    float sum = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        sum += data[i] * data[i];
    }
    return std::sqrt(sum / numSamples);
}

} // namespace ARC

