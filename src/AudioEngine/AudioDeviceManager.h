/*
 * ============================================================================
 * ARC System Audio Host - Audio Device Manager Header
 * Manages audio input/output devices and routing
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include <atomic>
#include <vector>
#include <functional>

namespace ARC
{

//=============================================================================
/**
 * Supported sample rates
 */
constexpr std::array<double, 4> SUPPORTED_SAMPLE_RATES = {44100.0, 48000.0, 88200.0, 96000.0};

/**
 * Supported buffer sizes
 */
constexpr std::array<int, 5> SUPPORTED_BUFFER_SIZES = {64, 128, 256, 512, 1024};

//=============================================================================
/**
 * Audio device information structure
 */
struct AudioDeviceInfo
{
    juce::String name;
    juce::String typeName;
    int numInputChannels{0};
    int numOutputChannels{0};
    juce::Array<double> sampleRates;
    juce::Array<int> bufferSizes;
    bool isDefault{false};
};

//=============================================================================
/**
 * Audio statistics for monitoring
 */
struct AudioStats
{
    double cpuUsage{0.0};
    double inputLatencyMs{0.0};
    double outputLatencyMs{0.0};
    double totalLatencyMs{0.0};
    int xrunCount{0};
    float peakLevelL{0.0f};
    float peakLevelR{0.0f};
    float rmsLevelL{0.0f};
    float rmsLevelR{0.0f};
    double currentSampleRate{0.0};
    int currentBufferSize{0};
};

//=============================================================================
/**
 * Callback interface for audio processing
 */
class AudioCallback
{
public:
    virtual ~AudioCallback() = default;
    virtual void processBlock(juce::AudioBuffer<float>& buffer,
                             juce::MidiBuffer& midiBuffer) = 0;
    virtual void prepareToPlay(double sampleRate, int samplesPerBlock) = 0;
    virtual void releaseResources() = 0;
};

//=============================================================================
/**
 * Audio Device Manager
 * Handles all audio device enumeration, initialization, and processing
 */
class AudioDeviceManager : public juce::AudioIODeviceCallback,
                           public juce::ChangeListener
{
public:
    //=========================================================================
    AudioDeviceManager();
    ~AudioDeviceManager() override;

    //=========================================================================
    // Initialization
    juce::String initialize(const juce::String& inputDevice,
                           const juce::String& outputDevice,
                           double sampleRate,
                           int bufferSize);

    void shutdown();

    //=========================================================================
    // Device enumeration
    std::vector<AudioDeviceInfo> getAvailableInputDevices() const;
    std::vector<AudioDeviceInfo> getAvailableOutputDevices() const;
    juce::StringArray getDeviceTypeNames() const;

    //=========================================================================
    // Device selection
    juce::String setInputDevice(const juce::String& deviceName);
    juce::String setOutputDevice(const juce::String& deviceName);
    juce::String setSampleRate(double sampleRate);
    juce::String setBufferSize(int bufferSize);

    //=========================================================================
    // Current state
    juce::String getCurrentInputDevice() const;
    juce::String getCurrentOutputDevice() const;
    double getCurrentSampleRate() const;
    int getCurrentBufferSize() const;

    //=========================================================================
    // Processing
    void setAudioCallback(AudioCallback* callback);
    void removeAudioCallback();

    //=========================================================================
    // Master volume and bypass
    void setMasterVolume(float volume);
    float getMasterVolume() const;
    void setMasterBypass(bool bypass);
    bool isMasterBypassed() const;

    //=========================================================================
    // Monitoring
    AudioStats getAudioStats() const;
    void resetXrunCount();

    //=========================================================================
    // Limiter
    void setLimiterEnabled(bool enabled);
    bool isLimiterEnabled() const;
    void setLimiterThreshold(float thresholdDb);

    //=========================================================================
    // Device change listener
    using DeviceChangeCallback = std::function<void()>;
    void addDeviceChangeCallback(DeviceChangeCallback callback);

    //=========================================================================
    // AudioIODeviceCallback interface
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceError(const juce::String& errorMessage) override;

    //=========================================================================
    // ChangeListener interface
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

private:
    //=========================================================================
    void updateAudioStats(int numSamples);
    void applyLimiter(juce::AudioBuffer<float>& buffer);
    float calculatePeakLevel(const float* data, int numSamples);
    float calculateRMSLevel(const float* data, int numSamples);

    //=========================================================================
    juce::AudioDeviceManager deviceManager;
    juce::AudioDeviceManager::AudioDeviceSetup currentSetup;

    // RT-safe: read with load(acquire) from the audio callback; written under
    // callbackMutex from Non-RT threads only.
    std::atomic<AudioCallback*> audioCallback{nullptr};

    std::atomic<float> masterVolume{1.0f};
    std::atomic<bool> masterBypass{false};
    std::atomic<bool> limiterEnabled{true};
    std::atomic<float> limiterThreshold{-0.3f}; // dB

    // Statistics
    mutable std::mutex statsMutex;
    AudioStats stats;
    std::atomic<int> xrunCount{0};

    // Level metering
    std::atomic<float> peakLevelL{0.0f};
    std::atomic<float> peakLevelR{0.0f};
    std::atomic<float> rmsLevelL{0.0f};
    std::atomic<float> rmsLevelR{0.0f};
    float peakDecay{0.9995f};

    // Limiter state
    float limiterGain{1.0f};
    static constexpr float LIMITER_ATTACK = 0.001f;
    static constexpr float LIMITER_RELEASE = 0.1f;

    // Callbacks
    std::vector<DeviceChangeCallback> deviceChangeCallbacks;
    std::mutex callbackMutex;

    // Processing buffer
    juce::AudioBuffer<float> processingBuffer;
    juce::MidiBuffer midiBuffer;

    // Timing for CPU measurement
    double lastCallbackTime{0.0};
    double cpuUsageSmoothed{0.0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioDeviceManager)
};

} // namespace ARC

