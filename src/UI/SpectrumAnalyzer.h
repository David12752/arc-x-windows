/*
 * ============================================================================
 * ARC System Audio Host - Spectrum Analyzer Header
 * Real-time frequency spectrum visualization
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include "../AudioEngine/AudioProcessor.h"
#include <array>
#include <atomic>

namespace ARC
{

//=============================================================================
/**
 * Spectrum analyzer component
 * Displays real-time frequency spectrum of audio
 */
class SpectrumAnalyzer : public juce::Component,
                         public juce::Timer
{
public:
    //=========================================================================
    enum class DisplayMode
    {
        Bars,
        Line,
        FilledLine
    };

    enum class ScaleMode
    {
        Linear,
        Logarithmic
    };

    //=========================================================================
    SpectrumAnalyzer();
    ~SpectrumAnalyzer() override;

    //=========================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    //=========================================================================
    // Data input
    void pushSamples(const float* samples, int numSamples);
    void setSpectrumData(const SpectrumData& data);

    //=========================================================================
    // Configuration
    void setDisplayMode(DisplayMode mode);
    void setScaleMode(ScaleMode mode);
    void setFrequencyRange(float minHz, float maxHz);
    void setDecibelRange(float minDb, float maxDb);
    void setSmoothingFactor(float factor);
    void setShowGrid(bool show);
    void setShowLabels(bool show);
    void setGradientColors(const juce::Colour& low, const juce::Colour& high);

    //=========================================================================
    // Sample rate (needed for frequency calculation)
    void setSampleRate(double rate) { sampleRate = rate; }

private:
    //=========================================================================
    void processFFT();
    void drawBackground(juce::Graphics& g);
    void drawGrid(juce::Graphics& g);
    void drawSpectrum(juce::Graphics& g);
    void drawLabels(juce::Graphics& g);
    float frequencyToX(float frequency) const;
    float magnitudeToY(float magnitude) const;

    //=========================================================================
    DisplayMode displayMode{DisplayMode::FilledLine};
    ScaleMode scaleMode{ScaleMode::Logarithmic};

    float minFrequency{20.0f};
    float maxFrequency{20000.0f};
    float minDecibels{-90.0f};
    float maxDecibels{0.0f};
    float smoothingFactor{0.7f};

    bool showGrid{true};
    bool showLabels{true};

    juce::Colour lowColour;
    juce::Colour highColour;

    double sampleRate{48000.0};

    // FFT
    static constexpr int FFT_ORDER = 11;
    static constexpr int FFT_SIZE = 1 << FFT_ORDER; // 2048
    static constexpr int NUM_BINS = FFT_SIZE / 2;

    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    std::array<float, FFT_SIZE * 2> fftData{};
    std::array<float, NUM_BINS> magnitudes{};
    std::array<float, NUM_BINS> smoothedMagnitudes{};

    int fftWriteIndex{0};
    std::atomic<bool> fftReady{false};

    // Display path (cached for performance)
    juce::Path spectrumPath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};

//=============================================================================
/**
 * Frequency response display (for showing ARC correction curve)
 */
class FrequencyResponseDisplay : public juce::Component
{
public:
    FrequencyResponseDisplay();

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Set response curve data (frequency -> dB pairs)
    void setResponseCurve(const std::vector<std::pair<float, float>>& curve);
    void clearResponseCurve();

    void setFrequencyRange(float minHz, float maxHz);
    void setDecibelRange(float minDb, float maxDb);
    void setShowReference(bool show);

private:
    std::vector<std::pair<float, float>> responseCurve;

    float minFrequency{20.0f};
    float maxFrequency{20000.0f};
    float minDecibels{-24.0f};
    float maxDecibels{24.0f};

    bool showReference{true};

    juce::Path curvePath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FrequencyResponseDisplay)
};

} // namespace ARC

