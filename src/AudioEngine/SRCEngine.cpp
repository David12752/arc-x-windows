/*
 * ============================================================================
 * ARC System Audio Host - Sample Rate Conversion Engine Implementation
 * ============================================================================
 */

#include "SRCEngine.h"
#include "../Utils/Logger.h"
#include <cmath>

namespace ARC
{

//=============================================================================
SRCEngine::SRCEngine()
{
    Logger::log("SRCEngine created");
}

//=============================================================================
void SRCEngine::reconfigure(double inRate, double outRate,
                              int channels, int maxInputBlockSize)
{
    // Mark as not ready while reconfiguring (RT thread will passthrough)
    ready.store(false, std::memory_order_release);

    inputRate.store(inRate,   std::memory_order_relaxed);
    outputRate.store(outRate, std::memory_order_relaxed);
    numChannels = juce::jlimit(1, MAX_CHANNELS, channels);

    // Conversion ratio: how many output samples per input sample
    const double r = (inRate > 0.0 && outRate > 0.0) ? (outRate / inRate) : 1.0;
    ratio.store(r, std::memory_order_relaxed);

    // Pre-allocate scratch output buffer.
    // Worst-case output samples for one block = ceil(maxInputBlockSize * ratio) + headroom
    const int maxOut = static_cast<int>(std::ceil(maxInputBlockSize * r)) + 8;
    for (int ch = 0; ch < MAX_CHANNELS; ++ch)
        scratchOut[ch].assign(static_cast<size_t>(juce::jmax(maxOut, MAX_OUTPUT_BLOCK)), 0.0f);

    // Reset interpolator state so artifacts don't carry over from previous config
    for (auto& interp : interpolators)
        interp.reset();

    ready.store(true, std::memory_order_release);

    const double ratioLog = r;
    Logger::log("SRCEngine configured: " +
                juce::String(inRate,  0) + " Hz -> " +
                juce::String(outRate, 0) + " Hz  (ratio = " +
                juce::String(ratioLog, 6) + ")" +
                (isConversionRequired() ? "  [SRC ACTIVE]" : "  [passthrough]"));
}

//=============================================================================
int SRCEngine::convert(const float* const* input, int numInputSamples,
                        float* const*       output, int maxOutputSamples)
{
    if (!ready.load(std::memory_order_acquire))
    {
        // Reconfiguration in progress — silent passthrough (zero output)
        const int copyLen = juce::jmin(numInputSamples, maxOutputSamples);
        for (int ch = 0; ch < numChannels; ++ch)
            juce::FloatVectorOperations::copy(output[ch], input[ch], copyLen);
        return copyLen;
    }

    const double r = ratio.load(std::memory_order_relaxed);

    // Fast path: rates match (within 0.1%) — memcopy, no interpolation
    if (!isConversionRequired())
    {
        const int copyLen = juce::jmin(numInputSamples, maxOutputSamples);
        for (int ch = 0; ch < numChannels; ++ch)
            juce::FloatVectorOperations::copy(output[ch], input[ch], copyLen);
        return copyLen;
    }

    // Compute how many output samples we expect to produce
    const int expectedOut = static_cast<int>(std::round(numInputSamples * r));
    const int targetOut   = juce::jlimit(1, maxOutputSamples, expectedOut);

    // CatmullRomInterpolator::process() signature:
    //   int process(double speedRatio, const float* inputSamples,
    //               float* outputSamples, int numOutputSamplesToProduce,
    //               int numInputSamplesAvailable = std::numeric_limits<int>::max())
    //
    // speedRatio = inputRate / outputRate  (< 1 means upsampling, > 1 downsampling)
    const double speedRatio = 1.0 / r; // = inputRate / outputRate

    int samplesWritten = 0;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        samplesWritten = interpolators[ch].process(
            speedRatio,
            input[ch],
            output[ch],
            targetOut,
            numInputSamples);
    }

    return samplesWritten;
}

//=============================================================================
bool SRCEngine::isConversionRequired() const noexcept
{
    const double r = ratio.load(std::memory_order_relaxed);
    // Consider rates matched if within 0.1% — handles minor clock drift
    return std::abs(r - 1.0) > 0.001;
}

//=============================================================================
int SRCEngine::expectedOutputSamples(int numInputSamples) const noexcept
{
    const double r = ratio.load(std::memory_order_relaxed);
    return static_cast<int>(std::ceil(numInputSamples * r));
}

//=============================================================================
void SRCEngine::reset()
{
    for (auto& interp : interpolators)
        interp.reset();

    Logger::log("SRCEngine reset");
}

} // namespace ARC
