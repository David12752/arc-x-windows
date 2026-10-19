/*
 * ============================================================================
 * ARC System Audio Host - VB-Audio Cable Wrapper Implementation
 * ============================================================================
 */

#include "VBCableWrapper.h"
#include "AudioDeviceManager.h"
#include "../Utils/Logger.h"

namespace ARC
{

//=============================================================================
bool VBCableWrapper::containsIgnoreCase(const juce::String& haystack,
                                        const juce::String& needle) noexcept
{
    return haystack.toLowerCase().contains(needle.toLowerCase());
}

//=============================================================================
VBCableInfo VBCableWrapper::detectVBCable(AudioDeviceManager& adm)
{
    VBCableInfo info;

    // --- Render devices (outputs) -------------------------------------------
    for (const auto& dev : adm.getAvailableOutputDevices())
    {
        if (containsIgnoreCase(dev.name, CABLE_RENDER_KEYWORD))
        {
            info.renderFound      = true;
            info.renderDeviceName = dev.name;
            Logger::log("VBCableWrapper: render endpoint found — \"" + dev.name + "\"");
            break;
        }
    }

    // --- Capture devices (inputs) --------------------------------------------
    for (const auto& dev : adm.getAvailableInputDevices())
    {
        if (containsIgnoreCase(dev.name, CABLE_CAPTURE_KEYWORD))
        {
            info.captureFound      = true;
            info.captureDeviceName = dev.name;
            Logger::log("VBCableWrapper: capture endpoint found — \"" + dev.name + "\"");
            break;
        }
    }

    if (!info.renderFound && !info.captureFound)
        Logger::log("VBCableWrapper: VB-Audio Virtual Cable not detected");
    else if (!info.isFullyAvailable())
        Logger::warning("VBCableWrapper: only one cable endpoint found — cable may be partially installed");

    return info;
}

//=============================================================================
bool VBCableWrapper::isInstalled(AudioDeviceManager& adm)
{
    return detectVBCable(adm).isFullyAvailable();
}

//=============================================================================
juce::String VBCableWrapper::getInstallGuidance(const VBCableInfo& info)
{
    if (info.isFullyAvailable())
    {
        return "VB-Audio Virtual Cable is installed and ready.\n"
               "Render: "  + info.renderDeviceName  + "\n"
               "Capture: " + info.captureDeviceName;
    }

    if (info.renderFound && !info.captureFound)
    {
        return "CABLE Input (render) was found but CABLE Output (capture) is missing.\n"
               "Try reinstalling VB-Audio Virtual Cable.";
    }

    if (!info.renderFound && info.captureFound)
    {
        return "CABLE Output (capture) was found but CABLE Input (render) is missing.\n"
               "Try reinstalling VB-Audio Virtual Cable.";
    }

    return "VB-Audio Virtual Cable is not installed.\n"
           "Download the free driver from: https://vb-audio.com/Cable/\n"
           "After installation, restart ARC to apply the new routing.";
}

//=============================================================================
juce::String VBCableWrapper::applyRouting(AudioDeviceManager& adm,
                                          double sampleRate,
                                          int    bufferSize)
{
    Logger::log("VBCableWrapper::applyRouting — detecting VB-Cable endpoints");

    const VBCableInfo info = detectVBCable(adm);

    if (!info.renderFound)
    {
        const juce::String msg = "VBCableWrapper::applyRouting failed: "
                                 "CABLE Input render device not found. "
                                 "Is VB-Audio Virtual Cable installed?";
        Logger::error(msg);
        return msg;
    }

    Logger::log("VBCableWrapper: applying routing — output=\""
                + info.renderDeviceName + "\"");

    // Delegate to ARC::AudioDeviceManager's high-level setter
    const juce::String err = adm.setOutputDevice(info.renderDeviceName);
    if (err.isNotEmpty())
    {
        Logger::error("VBCableWrapper: setOutputDevice error: " + err);
        return err;
    }

    Logger::log("VBCableWrapper: routing applied successfully");
    return {};
}

//=============================================================================
juce::String VBCableWrapper::revertToDefault(AudioDeviceManager& adm,
                                             double sampleRate,
                                             int    bufferSize)
{
    Logger::log("VBCableWrapper::revertToDefault — reverting to system default device");

    // Empty string = JUCE picks the system default
    const juce::String err = adm.setOutputDevice("");
    if (err.isNotEmpty())
        Logger::error("VBCableWrapper::revertToDefault error: " + err);
    else
        Logger::log("VBCableWrapper: reverted to default device");

    return err;
}

} // namespace ARC
