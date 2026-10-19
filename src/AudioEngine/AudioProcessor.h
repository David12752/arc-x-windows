/*
 * ============================================================================
 * ARC System Audio Host - Audio Processor Header
 * Core audio processing with DSP utilities
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include <atomic>
#include <array>

namespace ARC
{

//=============================================================================
/**
 * Spectrum analyzer data structure
 */
struct SpectrumData
{
    static constexpr int FFT_SIZE = 2048;
    static constexpr int NUM_BINS = FFT_SIZE / 2;

    std::array<float, NUM_BINS> magnitudes{};
    std::array<float, NUM_BINS> phases{};
    std::array<float, NUM_BINS> smoothedMagnitudes{};

    void reset()
    {
        magnitudes.fill(0.0f);
        phases.fill(0.0f);
        smoothedMagnitudes.fill(0.0f);
    }
};

//=============================================================================
/**
 * Audio level data for metering
 */
struct LevelData
{
    float peakL{0.0f};
    float peakR{0.0f};
    float rmsL{0.0f};
    float rmsR{0.0f};
    float peakHoldL{0.0f};
    float peakHoldR{0.0f};
    int peakHoldSamplesL{0};
    int peakHoldSamplesR{0};

    void decay(float peakDecay, float rmsDecay, int peakHoldTime)
    {
        peakL *= peakDecay;
        peakR *= peakDecay;
        rmsL *= rmsDecay;
        rmsR *= rmsDecay;

        if (--peakHoldSamplesL <= 0)
            peakHoldL *= peakDecay;
        if (--peakHoldSamplesR <= 0)
            peakHoldR *= peakDecay;
    }
};

//=============================================================================
/**
 * Biquad filter for EQ and processing
 */
class BiquadFilter
{
public:
    enum class Type
    {
        LowPass,
        HighPass,
        BandPass,
        Notch,
        Peak,
        LowShelf,
        HighShelf
    };

    BiquadFilter() = default;

    void setCoefficients(Type type, double sampleRate, double frequency,
                         double q = 0.707, double gainDb = 0.0);
    void reset();
    float process(float input);
    void processBlock(float* data, int numSamples);

private:
    double b0{1.0}, b1{0.0}, b2{0.0};
    double a1{0.0}, a2{0.0};
    double z1{0.0}, z2{0.0};
};

//=============================================================================
/**
 * True Look-Ahead Limiter.
 *
 * Algorithm:
 *   1. Incoming samples are written into a circular delay ring.
 *   2. For each output sample the limiter scans LOOKAHEAD_SAMPLES
 *      positions *ahead* in the ring to find the worst-case future peak.
 *   3. The required gain reduction is applied to the *delayed* (current
 *      output) sample — guaranteeing zero over-shoots at the output.
 *   4. Attack is instantaneous (the look-ahead window absorbs it);
 *      release is exponentially smoothed.
 *
 * Reported latency: LOOKAHEAD_SAMPLES (must be fed into PDC engine).
 */
class LookAheadLimiter
{
public:
    LookAheadLimiter();

    void prepare(double sampleRate, int samplesPerBlock);
    void setThreshold(float thresholdDb);
    void setRelease(float releaseMs);
    void process(juce::AudioBuffer<float>& buffer);
    void reset();

    float getGainReduction() const { return gainReduction.load(); }
    /** Latency introduced — report this to the PDC engine. */
    static constexpr int getLookaheadSamples() { return LOOKAHEAD_SAMPLES; }

private:
    static constexpr int LOOKAHEAD_SAMPLES = 64;
    // Ring buffer must be at least LOOKAHEAD_SAMPLES + maxBlockSize.
    // We allocate LOOKAHEAD_SAMPLES * 2 + 1024 to avoid resize at runtime.
    static constexpr int RING_CAPACITY = LOOKAHEAD_SAMPLES * 2 + 1024;

    double sampleRate{44100.0};
    float threshold{1.0f};      // linear
    float releaseCoeff{0.9999f};
    float currentGain{1.0f};
    std::atomic<float> gainReduction{0.0f};

    // Per-channel circular delay rings (heap-allocated once in prepare())
    // Index: [channel][sample]
    std::vector<std::vector<float>> ring;
    int numChannels{0};
    int writeHead{0};

    // Pre-allocated per-block gain envelope (avoids runtime allocation)
    std::vector<float> gainEnvelope;
};

//=============================================================================
/**
 * Main audio processor for system audio
 */
class AudioProcessor
{
public:
    //=========================================================================
    AudioProcessor();
    ~AudioProcessor();

    //=========================================================================
    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void process(juce::AudioBuffer<float>& buffer);
    void reset();

    //=========================================================================
    // Input processing
    void setInputGain(float gainDb);
    float getInputGain() const;

    //=========================================================================
    // Output processing
    void setOutputGain(float gainDb);
    float getOutputGain() const;

    //=========================================================================
    // Limiter
    void setLimiterEnabled(bool enabled);
    bool isLimiterEnabled() const;
    void setLimiterThreshold(float thresholdDb);
    float getLimiterGainReduction() const;

    //=========================================================================
    // DC filter
    void setDCFilterEnabled(bool enabled);
    bool isDCFilterEnabled() const;

    //=========================================================================
    // Metering
    LevelData getLevelData() const;
    void resetLevelData();

    //=========================================================================
    // Spectrum analysis
    bool isSpectrumReady() const;
    SpectrumData getSpectrumData();
    void enableSpectrum(bool enable);

    //=========================================================================
    // Latency
    int getLatencySamples() const;

private:
    //=========================================================================
    void updateLevels(const juce::AudioBuffer<float>& buffer);
    void updateSpectrum(const juce::AudioBuffer<float>& buffer);
    void applyDCFilter(juce::AudioBuffer<float>& buffer);

    //=========================================================================
    double currentSampleRate{44100.0};
    int currentBlockSize{512};
    int numChannels{2};
    bool prepared{false};

    // Gain
    juce::LinearSmoothedValue<float> inputGain;
    juce::LinearSmoothedValue<float> outputGain;

    // Limiter
    LookAheadLimiter limiter;
    std::atomic<bool> limiterEnabled{true};

    // DC filter
    std::atomic<bool> dcFilterEnabled{true};
    std::array<BiquadFilter, 2> dcFilters;

    // Metering
    mutable std::mutex levelMutex;
    LevelData levels;
    int peakHoldTime{48000}; // ~1 second at 48kHz

    // Spectrum
    std::atomic<bool> spectrumEnabled{true};
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, SpectrumData::FFT_SIZE * 2> fftData{};
    int fftWritePos{0};
    mutable std::mutex spectrumMutex;
    SpectrumData spectrumData;
    std::atomic<bool> spectrumReady{false};
    float spectrumSmoothing{0.8f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioProcessor)
};

} // namespace ARC

