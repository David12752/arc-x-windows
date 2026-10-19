/*
 * ============================================================================
 * ARC System Audio Host - VB-Audio Cable Wrapper
 *
 * Phase 4 Tier 1: Transparent routing via VB-Audio Virtual Cable.
 *
 * Routing model
 * -------------
 *   System audio → [CABLE Input (render)]  ← arc-engine output device
 *                ↓
 *   [CABLE Output (capture)] → WASAPILoopback → AudioPipeline → real speakers
 *
 * This means applications play to the virtual device (CABLE Input) as if
 * it were the default speaker, arc-engine captures that audio via WASAPI
 * loopback on CABLE Output, processes it through the VST chain, and writes
 * the result to the real physical output.
 *
 * Key responsibilities
 * --------------------
 *   - detectVBCable()       Enumerate WASAPI endpoints; find "CABLE Input"
 *                           (render) and "CABLE Output" (capture). Returns
 *                           a VBCableInfo describing what was found.
 *   - isInstalled()         Quick check — true when both endpoints are present.
 *   - getInstallGuidance()  Human-readable string for the UI.
 *   - applyRouting()        Set "CABLE Input" as the engine's output device
 *                           and "CABLE Output" as the loopback capture device
 *                           by writing to the ConfigManager and calling back
 *                           into EngineCore.
 *
 * VB-Audio cable device names
 * ---------------------------
 * VB-Audio Virtual Cable names vary slightly across versions but always
 * contain "CABLE" (case-insensitive).  We look for:
 *   - Render (output) device matching "CABLE Input"   → engine output
 *   - Capture (input) device matching "CABLE Output"  → loopback source
 *
 * Thread safety
 * -------------
 * All public methods are Non-RT and should only be called from the main/
 * command-server thread, never from the audio callback.
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include <optional>

// Forward declaration — avoids circular dependency
namespace ARC { class AudioDeviceManager; }

namespace ARC
{

//=============================================================================
/**
 * Result of a VB-Cable detection pass.
 */
struct VBCableInfo
{
    bool   renderFound  = false;   ///< "CABLE Input" render endpoint found
    bool   captureFound = false;   ///< "CABLE Output" capture endpoint found

    juce::String renderDeviceName;   ///< Full WASAPI name of the render endpoint
    juce::String captureDeviceName;  ///< Full WASAPI name of the capture endpoint

    /** True only when BOTH endpoints are present (cable fully usable). */
    bool isFullyAvailable() const noexcept { return renderFound && captureFound; }
};

//=============================================================================
/**
 * VBCableWrapper — detects and applies VB-Audio Virtual Cable routing.
 *
 * Stateless — all methods are const / static-style; the class holds no
 * persistent state of its own.  Callers should cache VBCableInfo themselves.
 */
class VBCableWrapper
{
public:
    VBCableWrapper()  = default;
    ~VBCableWrapper() = default;

    VBCableWrapper(const VBCableWrapper&)            = delete;
    VBCableWrapper& operator=(const VBCableWrapper&) = delete;

    //=========================================================================
    /**
     * Scan WASAPI render and capture device lists for VB-Audio Cable endpoints.
     *
     * Safe to call at any time after AudioDeviceManager::initialize().
     * Uses the ARC::AudioDeviceManager's enumeration methods — no direct
     * Win32 COM or raw JUCE AudioDeviceManager access needed.
     *
     * @param adm  The engine's ARC::AudioDeviceManager (already initialized).
     * @return VBCableInfo describing what was found.
     */
    static VBCableInfo detectVBCable(AudioDeviceManager& adm);

    //=========================================================================
    /**
     * Returns true if both CABLE Input and CABLE Output are present.
     */
    static bool isInstalled(AudioDeviceManager& adm);

    //=========================================================================
    /**
     * Human-readable guidance string for showing in the Settings panel.
     */
    static juce::String getInstallGuidance(const VBCableInfo& info);

    //=========================================================================
    /**
     * Apply VB-Cable routing: output = "CABLE Input", input = "CABLE Output".
     *
     * @param adm         The engine's AudioDeviceManager (already initialised).
     * @param sampleRate  Desired sample rate (unchanged).
     * @param bufferSize  Desired buffer size (unchanged).
     * @return Empty string on success; error description on failure.
     */
    static juce::String applyRouting(AudioDeviceManager& adm,
                                     double sampleRate,
                                     int    bufferSize);

    //=========================================================================
    /**
     * Revert routing back to system defaults.
     *
     * @return Empty string on success; error description on failure.
     */
    static juce::String revertToDefault(AudioDeviceManager& adm,
                                        double sampleRate,
                                        int    bufferSize);

    //=========================================================================
    // Name fragments used for matching (lowercase comparison)
    static constexpr const char* CABLE_RENDER_KEYWORD  = "cable input";
    static constexpr const char* CABLE_CAPTURE_KEYWORD = "cable output";

private:
    /** Case-insensitive substring search. */
    static bool containsIgnoreCase(const juce::String& haystack,
                                   const juce::String& needle) noexcept;
};

} // namespace ARC
