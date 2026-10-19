/*
 * ============================================================================
 * ARC System Audio Host - Virtual Audio Device Implementation
 * ============================================================================
 */

#include "VirtualAudioDevice.h"
#include "../Utils/Logger.h"

namespace ARC
{

//=============================================================================
VirtualAudioDeviceImpl::VirtualAudioDeviceImpl()
{
    Logger::log("VirtualAudioDeviceImpl created");
}

VirtualAudioDeviceImpl::~VirtualAudioDeviceImpl()
{
    uninstall();
    Logger::log("VirtualAudioDeviceImpl destroyed");
}

//=============================================================================
bool VirtualAudioDeviceImpl::install()
{
    if (state.load() == VirtualDeviceState::Installed ||
        state.load() == VirtualDeviceState::Running)
    {
        return true;
    }

    Logger::log("Installing virtual audio device...");
    state.store(VirtualDeviceState::Installing);

    // Check requirements
    if (!checkDriverRequirements())
    {
        state.store(VirtualDeviceState::Error);
        return false;
    }

    // Load the driver
    if (!loadDriver())
    {
        state.store(VirtualDeviceState::Error);
        return false;
    }

    state.store(VirtualDeviceState::Installed);
    Logger::log("Virtual audio device installed successfully");
    return true;
}

bool VirtualAudioDeviceImpl::uninstall()
{
    if (state.load() == VirtualDeviceState::NotInstalled)
    {
        return true;
    }

    Logger::log("Uninstalling virtual audio device...");

    unloadDriver();

    state.store(VirtualDeviceState::NotInstalled);
    Logger::log("Virtual audio device uninstalled");
    return true;
}

bool VirtualAudioDeviceImpl::isInstalled() const
{
    auto currentState = state.load();
    return currentState == VirtualDeviceState::Installed ||
           currentState == VirtualDeviceState::Running;
}

VirtualDeviceState VirtualAudioDeviceImpl::getState() const
{
    return state.load();
}

//=============================================================================
juce::String VirtualAudioDeviceImpl::getDeviceId() const
{
    return deviceId;
}

juce::String VirtualAudioDeviceImpl::getDeviceName() const
{
    return deviceName;
}

juce::String VirtualAudioDeviceImpl::getFriendlyName() const
{
    return friendlyName;
}

//=============================================================================
void VirtualAudioDeviceImpl::setSampleRate(double rate)
{
    sampleRate = rate;
}

double VirtualAudioDeviceImpl::getSampleRate() const
{
    return sampleRate;
}

void VirtualAudioDeviceImpl::setBufferSize(int size)
{
    bufferSize = size;
}

int VirtualAudioDeviceImpl::getBufferSize() const
{
    return bufferSize;
}

void VirtualAudioDeviceImpl::setChannelCount(int channels)
{
    channelCount = channels;
}

int VirtualAudioDeviceImpl::getChannelCount() const
{
    return channelCount;
}

//=============================================================================
juce::String VirtualAudioDeviceImpl::getLastError() const
{
    return lastError;
}

juce::String VirtualAudioDeviceImpl::getStatusMessage() const
{
    switch (state.load())
    {
        case VirtualDeviceState::NotInstalled:
            return "Virtual audio device not installed";
        case VirtualDeviceState::Installing:
            return "Installing virtual audio device...";
        case VirtualDeviceState::Installed:
            return "Virtual audio device ready";
        case VirtualDeviceState::Running:
            return "Virtual audio device running";
        case VirtualDeviceState::Error:
            return "Error: " + lastError;
        default:
            return "Unknown state";
    }
}

//=============================================================================
bool VirtualAudioDeviceImpl::checkDriverRequirements()
{
    // Check if running on Windows
#ifndef _WIN32
    lastError = "Virtual audio device is only available on Windows";
    return false;
#endif

    // In a real implementation, we would check:
    // - Windows version compatibility
    // - Administrative privileges
    // - Existing driver installations
    // - System audio configuration

    // For now, we'll indicate that external software is needed
    lastError = "A virtual audio cable driver is required. "
                "Please install VB-Audio Virtual Cable, VoiceMeeter, or similar software. "
                "This application will use WASAPI loopback for audio capture.";

    Logger::log("Virtual audio device check: " + lastError);

    // Return false to indicate we can't install our own driver
    // The application will fall back to WASAPI loopback
    return false;
}

bool VirtualAudioDeviceImpl::loadDriver()
{
    // In a real implementation, this would:
    // 1. Load a kernel-mode audio driver
    // 2. Register with Windows audio subsystem
    // 3. Create virtual audio endpoints

    Logger::log("Attempting to load virtual audio driver...");

    // Since we can't install a real driver without a signed .sys file,
    // we return false and the application uses WASAPI loopback instead

    lastError = "Driver loading not implemented - using WASAPI loopback fallback";
    return false;
}

void VirtualAudioDeviceImpl::unloadDriver()
{
    // Clean up any driver resources
    Logger::log("Unloading virtual audio driver...");
    deviceId = {};
}

} // namespace ARC

