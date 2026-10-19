/*
 * ============================================================================
 * ARC System Audio Host - Sample Rate Conversion Engine
 *
 * WHY THIS IS NEEDED
 * ------------------
 * WASAPI Loopback always returns audio in the Windows Audio Engine's mix
 * format — typically 48 kHz regardless of the ASIO/output device rate.
 * If the project sample rate differs from the loopback rate, the audio
 * will play back at the wrong pitch (ratio error = 2x octave for a 2:1
 * mismatch) with severe aliasing.
 *
 * ALGORITHM
 * ---------
 * Uses JUCE's built-in CatmullRomInterpolator for real-time conversion.
 * Quality trade-off: Catmull-Rom is a 4-point cubic interpolator — it
 * introduces very low HF roll-off and negligible latency (3–4 samples),
 * making it ideal for the RT audio path.
 *
 * For high-quality offline conversion (preset export, etc.) the caller
 * may use juce::LagrangeInterpolator or link libsamplerate.
 *
 * THREAD SAFETY
 * -------------
 * convert() is called from the RT audio thread.
 * reconfigure() is called from the Non-RT thread (device change).
 * The two are separated by an atomic "ready" flag; conversion is skipped
 * (passthrough) during the brief reconfiguration window.
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include <vector>
#include <array>
#include <atomic>

namespace ARC
{

//=============================================================================
/**
 * SRCEngine — Real-time sample-rate converter for the WASAPI loopback path.
 *
 * Typical usage:
 *
 *   // Non-RT (device change / init)
 *   srcEngine.reconfigure(wasapiRate, projectRate, numChannels, maxBlockIn);
 *
 *   // RT callback
 *   int outSamples = srcEngine.convert(inputBuffer, numIn, outputBuffer, maxOut);
 *   // outSamples is the number of valid samples written to outputBuffer
 */
class SRCEngine
{
public:
    //=========================================================================
    static constexpr int MAX_CHANNELS     = 2;
    static constexpr int MAX_OUTPUT_BLOCK = 8192;

    //=========================================================================
    SRCEngine();
    ~SRCEngine() = default;

    //=========================================================================
    /**
     * Non-RT: configure the converter.
     *
     * @param inputRate   Sample rate of the source (WASAPI loopback format).
     * @param outputRate  Sample rate of the engine / ASIO device.
     * @param numChannels Number of audio channels (1 or 2).
     * @param maxInputBlockSize  Largest block the loopback will ever deliver.
     *
     * Safe to call repeatedly; resets interpolator state each time.
     */
    void reconfigure(double inputRate, double outputRate,
                     int numChannels, int maxInputBlockSize);

    //=========================================================================
    /**
     * RT: convert `numInputSamples` from `input` into `output`.
     *
     * @param input           Array of per-channel input pointers (read-only).
     * @param numInputSamples Number of input samples per channel.
     * @param output          Array of per-channel output pointers (write).
     * @param maxOutputSamples Maximum capacity of each output channel pointer.
     *
     * @return Number of output samples written per channel.
     *         Returns `numInputSamples` (passthrough) when rates match.
     */
    int convert(const float* const* input, int numInputSamples,
                float* const*       output, int maxOutputSamples);

    //=========================================================================
    /** Returns true when input and output rates differ by more than 0.1 %. */
    bool isConversionRequired() const noexcept;

    /** Input rate as configured. */
    double getInputRate()  const noexcept { return inputRate.load();  }

    /** Output rate as configured. */
    double getOutputRate() const noexcept { return outputRate.load(); }

    /**
     * Expected output block size for a given input block size.
     * Useful for pre-allocating downstream buffers.
     */
    int expectedOutputSamples(int numInputSamples) const noexcept;

    //=========================================================================
    /** Non-RT: reset interpolator state (e.g. after a seek). */
    void reset();

private:
    //=========================================================================
    std::atomic<double> inputRate{48000.0};
    std::atomic<double> outputRate{48000.0};
    std::atomic<double> ratio{1.0};       // outputRate / inputRate
    std::atomic<bool>   ready{false};
    int numChannels{2};

    // One interpolator per channel — CatmullRom is a simple, low-latency
    // 4-point cubic interpolator built into JUCE.
    std::array<juce::CatmullRomInterpolator, MAX_CHANNELS> interpolators;

    // Intermediate scratch buffer (avoids runtime heap allocation)
    // Sized in reconfigure() to maxInputBlockSize * ceil(maxRatio)
    std::array<std::vector<float>, MAX_CHANNELS> scratchOut;
};

} // namespace ARC
