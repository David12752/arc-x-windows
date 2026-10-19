/*
 * ============================================================================
 * ARC System Audio Host - WASAPI Loopback Implementation
 * ============================================================================
 */

#include "WASAPILoopback.h"
#include "../Utils/Logger.h"

#ifdef _WIN32
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "avrt.lib")
#include <avrt.h>
#endif

namespace ARC
{

//=============================================================================
WASAPILoopback::WASAPILoopback()
{
    Logger::log("WASAPILoopback created");
}

WASAPILoopback::~WASAPILoopback()
{
    shutdown();
    Logger::log("WASAPILoopback destroyed");
}

//=============================================================================
bool WASAPILoopback::initialize()
{
#ifdef _WIN32
    if (initialized.load())
        return true;

    Logger::log("Initializing WASAPI loopback...");

    if (!initializeCOM())
    {
        lastError = "Failed to initialize COM";
        return false;
    }

    if (!enumerateDevices())
    {
        lastError = "Failed to enumerate audio devices";
        return false;
    }

    initialized.store(true);
    Logger::log("WASAPI loopback initialized successfully");
    return true;
#else
    lastError = "WASAPI is only available on Windows";
    return false;
#endif
}

void WASAPILoopback::shutdown()
{
#ifdef _WIN32
    if (!initialized.load())
        return;

    Logger::log("Shutting down WASAPI loopback...");

    stop();
    closeDevice();

    if (deviceEnumerator != nullptr)
    {
        deviceEnumerator->Release();
        deviceEnumerator = nullptr;
    }

    CoUninitialize();

    initialized.store(false);
    Logger::log("WASAPI loopback shutdown complete");
#endif
}

//=============================================================================
std::vector<WASAPIDeviceInfo> WASAPILoopback::getLoopbackDevices() const
{
    return loopbackDevices;
}

WASAPIDeviceInfo WASAPILoopback::getDefaultLoopbackDevice() const
{
    for (const auto& device : loopbackDevices)
    {
        if (device.isDefault)
            return device;
    }

    if (!loopbackDevices.empty())
        return loopbackDevices[0];

    return WASAPIDeviceInfo();
}

//=============================================================================
bool WASAPILoopback::setDevice(const juce::String& deviceId)
{
#ifdef _WIN32
    if (!initialized.load())
    {
        lastError = "Not initialized";
        return false;
    }

    bool wasRunning = running.load();
    if (wasRunning)
    {
        stop();
    }

    if (!openDevice(deviceId))
    {
        return false;
    }

    if (wasRunning)
    {
        return start();
    }

    return true;
#else
    juce::ignoreUnused(deviceId);
    return false;
#endif
}

juce::String WASAPILoopback::getCurrentDeviceId() const
{
    return currentDeviceId;
}

juce::String WASAPILoopback::getCurrentDeviceName() const
{
    return currentDeviceName;
}

//=============================================================================
bool WASAPILoopback::start()
{
#ifdef _WIN32
    if (!initialized.load())
    {
        lastError = "Not initialized";
        return false;
    }

    if (running.load())
        return true;

    if (currentDevice == nullptr)
    {
        // Open default device if none selected
        auto defaultDevice = getDefaultLoopbackDevice();
        if (!openDevice(defaultDevice.id))
        {
            return false;
        }
    }

    Logger::log("Starting WASAPI loopback capture...");

    // Initialize audio client for loopback
    HRESULT hr = currentDevice->Activate(
        __uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&audioClient);

    if (FAILED(hr))
    {
        lastError = "Failed to activate audio client";
        return false;
    }

    // Get mix format
    hr = audioClient->GetMixFormat(&mixFormat);
    if (FAILED(hr))
    {
        lastError = "Failed to get mix format";
        return false;
    }

    // Update current format
    currentFormat.numChannels = mixFormat->nChannels;
    currentFormat.bitsPerSample = mixFormat->wBitsPerSample;
    currentFormat.sampleRate = mixFormat->nSamplesPerSec;
    currentFormat.isFloat = (mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
        (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
         ((WAVEFORMATEXTENSIBLE*)mixFormat)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);

    // Initialize in loopback mode
    hr = audioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        0, 0, mixFormat, nullptr);

    if (FAILED(hr))
    {
        lastError = "Failed to initialize audio client in loopback mode";
        return false;
    }

    // Create event for capture
    captureEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    stopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    hr = audioClient->SetEventHandle(captureEvent);
    if (FAILED(hr))
    {
        lastError = "Failed to set event handle";
        return false;
    }

    // Get capture client
    hr = audioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&captureClient);
    if (FAILED(hr))
    {
        lastError = "Failed to get capture client";
        return false;
    }

    // Start capture
    hr = audioClient->Start();
    if (FAILED(hr))
    {
        lastError = "Failed to start audio client";
        return false;
    }

    running.store(true);

    // Start capture thread
    captureThreadHandle = std::thread(&WASAPILoopback::captureThread, this);

    Logger::log("WASAPI loopback capture started");
    return true;
#else
    return false;
#endif
}

void WASAPILoopback::stop()
{
#ifdef _WIN32
    if (!running.load())
        return;

    Logger::log("Stopping WASAPI loopback capture...");

    running.store(false);

    if (stopEvent != nullptr)
    {
        SetEvent(stopEvent);
    }

    if (captureThreadHandle.joinable())
    {
        captureThreadHandle.join();
    }

    if (audioClient != nullptr)
    {
        audioClient->Stop();
    }

    if (captureClient != nullptr)
    {
        captureClient->Release();
        captureClient = nullptr;
    }

    if (audioClient != nullptr)
    {
        audioClient->Release();
        audioClient = nullptr;
    }

    if (mixFormat != nullptr)
    {
        CoTaskMemFree(mixFormat);
        mixFormat = nullptr;
    }

    if (captureEvent != nullptr)
    {
        CloseHandle(captureEvent);
        captureEvent = nullptr;
    }

    if (stopEvent != nullptr)
    {
        CloseHandle(stopEvent);
        stopEvent = nullptr;
    }

    Logger::log("WASAPI loopback capture stopped");
#endif
}

bool WASAPILoopback::isRunning() const
{
    return running.load();
}

//=============================================================================
bool WASAPILoopback::setFormat(const WASAPIAudioFormat& format)
{
    currentFormat = format;
    return true;
}

WASAPIAudioFormat WASAPILoopback::getCurrentFormat() const
{
    return currentFormat;
}

int WASAPILoopback::getLatencyMs() const
{
#ifdef _WIN32
    if (audioClient != nullptr)
    {
        REFERENCE_TIME latency;
        if (SUCCEEDED(audioClient->GetStreamLatency(&latency)))
        {
            return static_cast<int>(latency / 10000);
        }
    }
#endif
    return 0;
}

//=============================================================================
void WASAPILoopback::setLoopbackQueue(LoopbackQueue* queue) noexcept
{
    loopbackQueue_.store(queue, std::memory_order_release);
    Logger::log(juce::String("WASAPILoopback: loopback queue ") +
                (queue ? "attached" : "detached"));
}

//=============================================================================
void WASAPILoopback::setAudioCallback(AudioCallback callback)
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    audioCallback = callback;
}

void WASAPILoopback::removeAudioCallback()
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    audioCallback = nullptr;
}

//=============================================================================
void WASAPILoopback::setProjectSampleRate(double projectRate)
{
    projectSampleRate = projectRate;

    if (initialized.load())
    {
        // Reconfigure the SRC engine with the current loopback format rate
        const int loopbackRate = currentFormat.sampleRate;
        const int numCh        = juce::jlimit(1, 2, currentFormat.numChannels);

        srcEngine.reconfigure(static_cast<double>(loopbackRate),
                              projectRate,
                              numCh,
                              /*maxInputBlock=*/4096);

        // Pre-allocate SRC output buffers
        const int maxOut = srcEngine.expectedOutputSamples(4096);
        for (int ch = 0; ch < numCh; ++ch)
        {
            srcOutputBuffer[ch].assign(static_cast<size_t>(juce::jmax(maxOut, 8192)), 0.0f);
            srcOutputPtrs[ch] = srcOutputBuffer[ch].data();
        }

        Logger::log("WASAPILoopback: SRC " +
                    juce::String(loopbackRate) + " Hz -> " +
                    juce::String(projectRate, 0) + " Hz  active=" +
                    (srcEngine.isConversionRequired() ? "YES" : "NO"));
    }
}

bool WASAPILoopback::isSRCActive() const
{
    return srcEngine.isConversionRequired();
}

//=============================================================================
juce::String WASAPILoopback::getLastError() const
{
    return lastError;
}

//=============================================================================
#ifdef _WIN32

bool WASAPILoopback::initializeCOM()
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    return SUCCEEDED(hr) || hr == S_FALSE || hr == RPC_E_CHANGED_MODE;
}

bool WASAPILoopback::enumerateDevices()
{
    loopbackDevices.clear();

    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&deviceEnumerator);

    if (FAILED(hr))
    {
        return false;
    }

    // Get default device
    IMMDevice* defaultDevice = nullptr;
    hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice);

    juce::String defaultDeviceId;
    if (SUCCEEDED(hr) && defaultDevice != nullptr)
    {
        LPWSTR id = nullptr;
        if (SUCCEEDED(defaultDevice->GetId(&id)))
        {
            defaultDeviceId = juce::String(id);
            CoTaskMemFree(id);
        }
        defaultDevice->Release();
    }

    // Enumerate render devices (for loopback)
    IMMDeviceCollection* deviceCollection = nullptr;
    hr = deviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &deviceCollection);

    if (FAILED(hr))
    {
        return false;
    }

    UINT count = 0;
    deviceCollection->GetCount(&count);

    for (UINT i = 0; i < count; ++i)
    {
        IMMDevice* device = nullptr;
        if (SUCCEEDED(deviceCollection->Item(i, &device)))
        {
            WASAPIDeviceInfo info;

            // Get device ID
            LPWSTR id = nullptr;
            if (SUCCEEDED(device->GetId(&id)))
            {
                info.id = juce::String(id);
                CoTaskMemFree(id);
            }

            // Get device properties
            IPropertyStore* props = nullptr;
            if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props)))
            {
                PROPVARIANT varName;
                PropVariantInit(&varName);
                if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName)))
                {
                    info.name = juce::String(varName.pwszVal);
                    PropVariantClear(&varName);
                }
                props->Release();
            }

            info.isLoopbackCapable = true;
            info.isDefault = (info.id == defaultDeviceId);

            // Get channel count
            IAudioClient* tempClient = nullptr;
            if (SUCCEEDED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
                                           nullptr, (void**)&tempClient)))
            {
                WAVEFORMATEX* format = nullptr;
                if (SUCCEEDED(tempClient->GetMixFormat(&format)))
                {
                    info.numChannels = format->nChannels;
                    info.defaultSampleRate = format->nSamplesPerSec;
                    CoTaskMemFree(format);
                }
                tempClient->Release();
            }

            loopbackDevices.push_back(info);
            device->Release();
        }
    }

    deviceCollection->Release();

    Logger::log("Found " + juce::String(loopbackDevices.size()) + " loopback devices");
    return true;
}

bool WASAPILoopback::openDevice(const juce::String& deviceId)
{
    closeDevice();

    if (deviceId.isEmpty())
    {
        // Use default device
        HRESULT hr = deviceEnumerator->GetDefaultAudioEndpoint(
            eRender, eConsole, &currentDevice);

        if (FAILED(hr))
        {
            lastError = "Failed to get default audio endpoint";
            return false;
        }
    }
    else
    {
        HRESULT hr = deviceEnumerator->GetDevice(
            deviceId.toWideCharPointer(), &currentDevice);

        if (FAILED(hr))
        {
            lastError = "Failed to get audio device: " + deviceId;
            return false;
        }
    }

    currentDeviceId = deviceId;

    // Get device name
    IPropertyStore* props = nullptr;
    if (SUCCEEDED(currentDevice->OpenPropertyStore(STGM_READ, &props)))
    {
        PROPVARIANT varName;
        PropVariantInit(&varName);
        if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName)))
        {
            currentDeviceName = juce::String(varName.pwszVal);
            PropVariantClear(&varName);
        }
        props->Release();
    }

    Logger::log("Opened loopback device: " + currentDeviceName);
    return true;
}

void WASAPILoopback::closeDevice()
{
    if (currentDevice != nullptr)
    {
        currentDevice->Release();
        currentDevice = nullptr;
    }
    currentDeviceId = {};
    currentDeviceName = {};
}

void WASAPILoopback::captureThread()
{
    // Set thread priority for audio processing
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    // Use MMCSS for pro audio
    DWORD taskIndex = 0;
    HANDLE taskHandle = AvSetMmThreadCharacteristics(L"Pro Audio", &taskIndex);

    Logger::log("Capture thread started");

    HANDLE events[] = { captureEvent, stopEvent };

    while (running.load())
    {
        DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, 100);

        if (waitResult == WAIT_OBJECT_0)
        {
            processAudioPacket();
        }
        else if (waitResult == WAIT_OBJECT_0 + 1)
        {
            break;
        }
    }

    if (taskHandle != nullptr)
    {
        AvRevertMmThreadCharacteristics(taskHandle);
    }

    Logger::log("Capture thread stopped");
}

bool WASAPILoopback::processAudioPacket()
{
    if (captureClient == nullptr)
        return false;

    UINT32 packetLength = 0;
    HRESULT hr = captureClient->GetNextPacketSize(&packetLength);

    while (SUCCEEDED(hr) && packetLength > 0)
    {
        BYTE* data = nullptr;
        UINT32 numFrames = 0;
        DWORD flags = 0;

        hr = captureClient->GetBuffer(&data, &numFrames, &flags, nullptr, nullptr);

        if (SUCCEEDED(hr))
        {
            // Check for silence
            if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
            {
                // Generate silence
                int numSamples = numFrames * currentFormat.numChannels;
                conversionBuffer.resize(numSamples);
                std::fill(conversionBuffer.begin(), conversionBuffer.end(), 0.0f);
            }
            else
            {
                // Convert audio data
                int numSamples = numFrames * currentFormat.numChannels;
                conversionBuffer.resize(numSamples);
                convertToFloat(data, conversionBuffer.data(), numFrames, currentFormat.numChannels);
            }

            // Build de-interleaved channel pointers
            // conversionBuffer is interleaved [L0,R0,L1,R1,...] — de-interleave here.
            const int numCh = juce::jlimit(1, 2, currentFormat.numChannels);
            channelPointers.resize(numCh);
            deinterleavedBuffer.resize(numCh);
            for (int ch = 0; ch < numCh; ++ch)
            {
                deinterleavedBuffer[ch].resize(static_cast<size_t>(numFrames));
                for (int s = 0; s < static_cast<int>(numFrames); ++s)
                    deinterleavedBuffer[ch][s] = conversionBuffer[s * numCh + ch];
                channelPointers[ch] = deinterleavedBuffer[ch].data();
            }

            // SRC: convert loopback rate → project rate if they differ
            const float* const* callbackInput = channelPointers.data();
            int callbackFrames = static_cast<int>(numFrames);

            if (srcEngine.isConversionRequired())
            {
                callbackFrames = srcEngine.convert(
                    channelPointers.data(),
                    static_cast<int>(numFrames),
                    srcOutputPtrs.data(),
                    static_cast<int>(srcOutputBuffer[0].size()));

                callbackInput = const_cast<const float* const*>(
                    reinterpret_cast<float* const*>(srcOutputPtrs.data()));
            }

            // ----------------------------------------------------------------
            // Dispatch captured audio to downstream consumers.
            // Priority 1: RT-safe lock-free queue (arc-engine path).
            // Priority 2: legacy std::function callback (GUI app path).
            // ----------------------------------------------------------------
            LoopbackQueue* q = loopbackQueue_.load(std::memory_order_acquire);
            if (q != nullptr)
            {
                AudioBlock block;
                block.fill(callbackInput, numCh, callbackFrames, projectSampleRate);
#ifdef _WIN32
                block.captureTimeMs = GetTickCount64();
#endif
                // Mono capture → duplicate to stereo so downstream always sees 2ch
                block.monoToStereo();

                // Drop oldest if full (capture thread is the sole producer)
                if (!q->try_enqueue(block))
                {
                    // Queue full — this is a downstream overrun.
                    // We could log here but Logger may allocate; use atomic xrun
                    // counter in EngineCore instead.
                }
            }
            else
            {
                // Legacy callback path (GUI Application)
                std::lock_guard<std::mutex> lock(callbackMutex);
                if (audioCallback)
                    audioCallback(callbackInput, numCh, callbackFrames);
            }

            hr = captureClient->ReleaseBuffer(numFrames);
        }

        hr = captureClient->GetNextPacketSize(&packetLength);
    }

    return true;
}

void WASAPILoopback::convertToFloat(const BYTE* data, float* output,
                                    int numSamples, int numChannels)
{
    if (currentFormat.isFloat && currentFormat.bitsPerSample == 32)
    {
        // Direct float copy
        std::memcpy(output, data, numSamples * numChannels * sizeof(float));
    }
    else if (currentFormat.bitsPerSample == 16)
    {
        // Convert 16-bit to float
        const int16_t* input16 = reinterpret_cast<const int16_t*>(data);
        int totalSamples = numSamples * numChannels;
        for (int i = 0; i < totalSamples; ++i)
        {
            output[i] = input16[i] / 32768.0f;
        }
    }
    else if (currentFormat.bitsPerSample == 24)
    {
        // Convert 24-bit to float
        int totalSamples = numSamples * numChannels;
        for (int i = 0; i < totalSamples; ++i)
        {
            int32_t sample = (data[i * 3] |
                             (data[i * 3 + 1] << 8) |
                             (data[i * 3 + 2] << 16));
            if (sample & 0x800000)
                sample |= 0xFF000000;
            output[i] = sample / 8388608.0f;
        }
    }
    else if (currentFormat.bitsPerSample == 32 && !currentFormat.isFloat)
    {
        // Convert 32-bit int to float
        const int32_t* input32 = reinterpret_cast<const int32_t*>(data);
        int totalSamples = numSamples * numChannels;
        for (int i = 0; i < totalSamples; ++i)
        {
            output[i] = input32[i] / 2147483648.0f;
        }
    }
}

#endif // _WIN32

//=============================================================================
// VirtualAudioDevice Implementation
//=============================================================================

VirtualAudioDevice::VirtualAudioDevice()
{
    Logger::log("VirtualAudioDevice created");
}

VirtualAudioDevice::~VirtualAudioDevice()
{
    uninstall();
    Logger::log("VirtualAudioDevice destroyed");
}

bool VirtualAudioDevice::install()
{
    Logger::log("Installing virtual audio device...");

    // Note: Installing a virtual audio device requires a kernel-mode driver
    // This would typically use something like:
    // - Windows Audio Device Graph Isolation
    // - A custom virtual audio cable driver
    // - VAC (Virtual Audio Cable)

    // For this implementation, we'll return false and suggest using
    // existing virtual audio cable software

    lastError = "Virtual audio device installation requires administrative privileges "
                "and a signed driver. Please use VB-Audio Virtual Cable or similar.";

    Logger::log(lastError);
    return false;
}

bool VirtualAudioDevice::uninstall()
{
    if (!installed)
        return true;

    Logger::log("Uninstalling virtual audio device...");
    installed = false;
    return true;
}

bool VirtualAudioDevice::isInstalled() const
{
    return installed;
}

juce::String VirtualAudioDevice::getDeviceName() const
{
    return deviceName;
}

juce::String VirtualAudioDevice::getLastError() const
{
    return lastError;
}

} // namespace ARC

