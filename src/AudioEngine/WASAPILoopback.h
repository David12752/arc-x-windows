/*
 * ============================================================================
 * ARC System Audio Host - WASAPI Loopback Header
 * System audio capture using Windows Audio Session API
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include "SRCEngine.h"
#include "../Concurrency/AudioBlock.h"
#include "../Concurrency/SPSCQueue.h"

#ifdef _WIN32
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <comdef.h>
#endif

#include <atomic>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace ARC
{

//=============================================================================
/**
 * WASAPI device information
 */
struct WASAPIDeviceInfo
{
    juce::String id;
    juce::String name;
    int numChannels{0};
    double defaultSampleRate{0.0};
    bool isLoopbackCapable{false};
    bool isDefault{false};
};

//=============================================================================
/**
 * WASAPI audio format
 */
struct WASAPIAudioFormat
{
    int numChannels{2};
    int bitsPerSample{32};
    int sampleRate{48000};
    bool isFloat{true};
};

//=============================================================================
/**
 * WASAPI Loopback capture class
 * Captures system audio output for processing
 */
class WASAPILoopback
{
public:
    //=========================================================================
    using AudioCallback = std::function<void(const float* const*, int, int)>;

    //=========================================================================
    WASAPILoopback();
    ~WASAPILoopback();

    //=========================================================================
    // Initialization
    bool initialize();
    void shutdown();

    //=========================================================================
    // Device enumeration
    std::vector<WASAPIDeviceInfo> getLoopbackDevices() const;
    WASAPIDeviceInfo getDefaultLoopbackDevice() const;

    //=========================================================================
    // Device selection
    bool setDevice(const juce::String& deviceId);
    juce::String getCurrentDeviceId() const;
    juce::String getCurrentDeviceName() const;

    //=========================================================================
    // Capture control
    bool start();
    void stop();
    bool isRunning() const;

    //=========================================================================
    // Configuration
    bool setFormat(const WASAPIAudioFormat& format);
    WASAPIAudioFormat getCurrentFormat() const;
    int getLatencyMs() const;

    //=========================================================================
    // Sample Rate Conversion
    // Call this after the output device is opened so the SRC engine knows
    // what rate the downstream pipeline expects.
    void setProjectSampleRate(double projectRate);
    bool isSRCActive() const;

    //=========================================================================
    // Lock-free loopback queue (preferred over callback for RT-safe architecture).
    // Set this before start(); the capture thread will push AudioBlocks here
    // instead of (or in addition to) firing the legacy AudioCallback.
    //
    // Queue ownership stays with the caller (typically EngineCore).
    // Pass nullptr to disable queue mode and fall back to the callback.
    using LoopbackQueue = SPSCQueue<AudioBlock, 16>;
    void setLoopbackQueue(LoopbackQueue* queue) noexcept;

    //=========================================================================
    // Callback (legacy — used by the GUI application path)
    void setAudioCallback(AudioCallback callback);
    void removeAudioCallback();

    //=========================================================================
    // Error handling
    juce::String getLastError() const;

private:
    //=========================================================================
#ifdef _WIN32
    bool initializeCOM();
    bool enumerateDevices();
    bool openDevice(const juce::String& deviceId);
    void closeDevice();
    void captureThread();
    bool processAudioPacket();
    void convertToFloat(const BYTE* data, float* output, int numSamples, int numChannels);

    //=========================================================================
    IMMDeviceEnumerator* deviceEnumerator{nullptr};
    IMMDevice* currentDevice{nullptr};
    IAudioClient* audioClient{nullptr};
    IAudioCaptureClient* captureClient{nullptr};

    WAVEFORMATEX* mixFormat{nullptr};
    HANDLE captureEvent{nullptr};
    HANDLE stopEvent{nullptr};
#endif

    //=========================================================================
    std::thread captureThreadHandle;
    std::atomic<bool> running{false};
    std::atomic<bool> initialized{false};

    AudioCallback audioCallback;
    std::mutex callbackMutex;

    // RT-safe alternative to the callback: push AudioBlocks into this queue.
    // Pointer is set once before start() and never changes during capture.
    std::atomic<LoopbackQueue*> loopbackQueue_{nullptr};

    WASAPIAudioFormat currentFormat;
    juce::String currentDeviceId;
    juce::String currentDeviceName;
    juce::String lastError;

    std::vector<WASAPIDeviceInfo> loopbackDevices;

    // Conversion buffer
    std::vector<float> conversionBuffer;
    std::vector<const float*> channelPointers;
    // De-interleaved per-channel scratch buffers (avoids runtime heap allocation)
    std::array<std::vector<float>, 2> deinterleavedBuffer;

    // SRC: converts loopback format rate -> project/ASIO rate
    SRCEngine srcEngine;
    double projectSampleRate{48000.0};
    // Output buffer for SRC (pre-allocated in start())
    std::array<std::vector<float>, 2> srcOutputBuffer;
    std::array<float*, 2>             srcOutputPtrs{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WASAPILoopback)
};

//=============================================================================
/**
 * Virtual audio device manager
 * Creates a virtual audio device for audio routing
 */
class VirtualAudioDevice
{
public:
    //=========================================================================
    VirtualAudioDevice();
    ~VirtualAudioDevice();

    //=========================================================================
    bool install();
    bool uninstall();
    bool isInstalled() const;

    //=========================================================================
    juce::String getDeviceName() const;
    juce::String getLastError() const;

private:
    bool installed{false};
    juce::String lastError;
    juce::String deviceName{"ARC Virtual Audio"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VirtualAudioDevice)
};

} // namespace ARC

