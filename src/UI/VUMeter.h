/*
 * ============================================================================
 * ARC System Audio Host - VU Meter Header
 * Audio level visualization component
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include <atomic>

namespace ARC
{

//=============================================================================
/**
 * VU Meter component for audio level visualization
 */
class VUMeter : public juce::Component,
                public juce::Timer
{
public:
    //=========================================================================
    enum class Orientation
    {
        Vertical,
        Horizontal
    };

    enum class MeterType
    {
        Peak,
        RMS,
        PeakAndRMS
    };

    //=========================================================================
    VUMeter();
    ~VUMeter() override;

    //=========================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    //=========================================================================
    // Level setting
    void setLevel(float level);
    void setLevelDecibels(float levelDb);
    void setLevels(float peak, float rms);

    //=========================================================================
    // Configuration
    void setOrientation(Orientation newOrientation);
    void setMeterType(MeterType type);
    void setRange(float minDb, float maxDb);
    void setPeakHoldTime(int milliseconds);
    void setDecayRate(float decayPerSecond);

    //=========================================================================
    // Visual options
    void setShowScale(bool show);
    void setShowPeakHold(bool show);
    void setGradientColors(const juce::Colour& low,
                           const juce::Colour& mid,
                           const juce::Colour& high);

private:
    //=========================================================================
    float levelToPosition(float levelDb) const;
    void drawMeterBackground(juce::Graphics& g);
    void drawMeterLevel(juce::Graphics& g);
    void drawPeakHold(juce::Graphics& g);
    void drawScale(juce::Graphics& g);
    juce::Colour getLevelColour(float position) const;

    //=========================================================================
    Orientation orientation{Orientation::Vertical};
    MeterType meterType{MeterType::Peak};

    std::atomic<float> currentLevel{0.0f};
    std::atomic<float> currentRMS{0.0f};
    float displayLevel{0.0f};
    float displayRMS{0.0f};
    float peakHoldLevel{0.0f};
    int peakHoldSamples{0};

    float minDb{-60.0f};
    float maxDb{6.0f};
    float decayRate{0.95f};
    int peakHoldTime{2000}; // ms

    bool showScale{false};
    bool showPeakHold{true};

    juce::Colour lowColour;
    juce::Colour midColour;
    juce::Colour highColour;
    juce::Colour backgroundColour;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VUMeter)
};

//=============================================================================
/**
 * Stereo VU meter pair
 */
class StereoVUMeter : public juce::Component
{
public:
    StereoVUMeter();

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setLevels(float leftPeak, float rightPeak);
    void setLevels(float leftPeak, float rightPeak, float leftRMS, float rightRMS);

    void setOrientation(VUMeter::Orientation orientation);

private:
    std::unique_ptr<VUMeter> leftMeter;
    std::unique_ptr<VUMeter> rightMeter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoVUMeter)
};

} // namespace ARC

