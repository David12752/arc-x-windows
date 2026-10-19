/*
 * ============================================================================
 * ARC System Audio Host - Virtual Audio Device Header
 * Virtual audio device for audio routing
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include <memory>
#include <atomic>

namespace ARC
{

//=============================================================================
/**
 * Virtual audio device state
 */
enum class VirtualDeviceState
{
    NotInstalled,
    Installing,
    Installed,
    Running,
    Error
};

//=============================================================================
/**
 * Virtual Audio Device
 * Creates a virtual audio device that captures system audio
 */
class VirtualAudioDeviceImpl
{
public:
    //=========================================================================
    VirtualAudioDeviceImpl();
    ~VirtualAudioDeviceImpl();

    //=========================================================================
    // Installation
    bool install();
    bool uninstall();
    bool isInstalled() const;
    VirtualDeviceState getState() const;

    //=========================================================================
    // Device info
    juce::String getDeviceId() const;
    juce::String getDeviceName() const;
    juce::String getFriendlyName() const;

    //=========================================================================
    // Audio configuration
    void setSampleRate(double sampleRate);
    double getSampleRate() const;
    void setBufferSize(int bufferSize);
    int getBufferSize() const;
    void setChannelCount(int channels);
    int getChannelCount() const;

    //=========================================================================
    // Error handling
    juce::String getLastError() const;
    juce::String getStatusMessage() const;

private:
    //=========================================================================
    bool checkDriverRequirements();
    bool loadDriver();
    void unloadDriver();

    //=========================================================================
    std::atomic<VirtualDeviceState> state{VirtualDeviceState::NotInstalled};

    juce::String deviceId;
    juce::String deviceName{"ARC Virtual Audio Device"};
    juce::String friendlyName{"ARC System Audio"};

    double sampleRate{48000.0};
    int bufferSize{256};
    int channelCount{2};

    juce::String lastError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VirtualAudioDeviceImpl)
};

} // namespace ARC

