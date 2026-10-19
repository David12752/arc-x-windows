/*
 * ============================================================================
 * ARC System Audio Host - VU Meter Implementation
 * ============================================================================
 */

#include "VUMeter.h"
#include "LookAndFeel.h"

namespace ARC
{

//=============================================================================
VUMeter::VUMeter()
    : lowColour(Colors::meterLow),
      midColour(Colors::meterMid),
      highColour(Colors::meterHigh),
      backgroundColour(Colors::meterBackground)
{
    startTimerHz(60);
}

VUMeter::~VUMeter()
{
    stopTimer();
}

void VUMeter::paint(juce::Graphics& g)
{
    drawMeterBackground(g);
    drawMeterLevel(g);

    if (showPeakHold)
    {
        drawPeakHold(g);
    }

    if (showScale)
    {
        drawScale(g);
    }
}

void VUMeter::resized()
{
}

void VUMeter::timerCallback()
{
    // Get current levels
    float targetLevel = currentLevel.load();
    float targetRMS = currentRMS.load();

    // Smooth decay
    if (displayLevel > targetLevel)
    {
        displayLevel *= decayRate;
        if (displayLevel < 0.001f)
            displayLevel = 0.0f;
    }
    else
    {
        displayLevel = targetLevel;
    }

    if (displayRMS > targetRMS)
    {
        displayRMS *= decayRate;
    }
    else
    {
        displayRMS = targetRMS;
    }

    // Update peak hold
    if (displayLevel > peakHoldLevel)
    {
        peakHoldLevel = displayLevel;
        peakHoldSamples = static_cast<int>(peakHoldTime * 0.06f); // 60fps
    }
    else if (peakHoldSamples > 0)
    {
        --peakHoldSamples;
    }
    else
    {
        peakHoldLevel *= 0.95f;
    }

    repaint();
}

void VUMeter::setLevel(float level)
{
    currentLevel.store(juce::jlimit(0.0f, 1.5f, level));
}

void VUMeter::setLevelDecibels(float levelDb)
{
    float normalized = (levelDb - minDb) / (maxDb - minDb);
    setLevel(juce::jlimit(0.0f, 1.0f, normalized));
}

void VUMeter::setLevels(float peak, float rms)
{
    currentLevel.store(juce::jlimit(0.0f, 1.5f, peak));
    currentRMS.store(juce::jlimit(0.0f, 1.5f, rms));
}

void VUMeter::setOrientation(Orientation newOrientation)
{
    orientation = newOrientation;
    repaint();
}

void VUMeter::setMeterType(MeterType type)
{
    meterType = type;
    repaint();
}

void VUMeter::setRange(float newMinDb, float newMaxDb)
{
    minDb = newMinDb;
    maxDb = newMaxDb;
    repaint();
}

void VUMeter::setPeakHoldTime(int milliseconds)
{
    peakHoldTime = milliseconds;
}

void VUMeter::setDecayRate(float rate)
{
    decayRate = rate;
}

void VUMeter::setShowScale(bool show)
{
    showScale = show;
    repaint();
}

void VUMeter::setShowPeakHold(bool show)
{
    showPeakHold = show;
    repaint();
}

void VUMeter::setGradientColors(const juce::Colour& low,
                                 const juce::Colour& mid,
                                 const juce::Colour& high)
{
    lowColour = low;
    midColour = mid;
    highColour = high;
    repaint();
}

float VUMeter::levelToPosition(float level) const
{
    return juce::jlimit(0.0f, 1.0f, level);
}

void VUMeter::drawMeterBackground(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1);

    g.setColour(backgroundColour);
    g.fillRoundedRectangle(bounds, 3.0f);

    g.setColour(Colors::border);
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
}

void VUMeter::drawMeterLevel(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2);

    float position = levelToPosition(displayLevel);

    if (position <= 0.0f)
        return;

    juce::Rectangle<float> levelBounds;

    if (orientation == Orientation::Horizontal)
    {
        float width = bounds.getWidth() * position;
        levelBounds = bounds.withWidth(width);
    }
    else
    {
        float height = bounds.getHeight() * position;
        levelBounds = bounds.removeFromBottom(height);
    }

    // Create gradient
    juce::ColourGradient gradient;

    if (orientation == Orientation::Horizontal)
    {
        gradient = juce::ColourGradient(lowColour, bounds.getX(), 0,
                                        highColour, bounds.getRight(), 0, false);
    }
    else
    {
        gradient = juce::ColourGradient(lowColour, 0, bounds.getBottom(),
                                        highColour, 0, bounds.getY(), false);
    }

    gradient.addColour(0.6, midColour);
    gradient.addColour(0.85, midColour);

    g.setGradientFill(gradient);
    g.fillRoundedRectangle(levelBounds, 2.0f);

    // Draw RMS if enabled
    if (meterType == MeterType::PeakAndRMS && displayRMS > 0)
    {
        float rmsPosition = levelToPosition(displayRMS);
        juce::Rectangle<float> rmsBounds;

        if (orientation == Orientation::Horizontal)
        {
            float width = bounds.getWidth() * rmsPosition;
            rmsBounds = bounds.withWidth(width).reduced(0, 2);
        }
        else
        {
            float height = bounds.getHeight() * rmsPosition;
            rmsBounds = bounds.removeFromBottom(height).reduced(2, 0);
        }

        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.fillRoundedRectangle(rmsBounds, 2.0f);
    }
}

void VUMeter::drawPeakHold(juce::Graphics& g)
{
    if (peakHoldLevel <= 0.0f)
        return;

    auto bounds = getLocalBounds().toFloat().reduced(2);
    float position = levelToPosition(peakHoldLevel);

    g.setColour(getLevelColour(position).brighter(0.3f));

    if (orientation == Orientation::Horizontal)
    {
        float x = bounds.getX() + bounds.getWidth() * position - 2;
        g.fillRect(x, bounds.getY(), 2.0f, bounds.getHeight());
    }
    else
    {
        float y = bounds.getBottom() - bounds.getHeight() * position;
        g.fillRect(bounds.getX(), y, bounds.getWidth(), 2.0f);
    }
}

void VUMeter::drawScale(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(Colors::textDim);
    g.setFont(juce::Font(10.0f));

    // Draw scale marks at -60, -40, -20, -10, -6, -3, 0, +3, +6 dB
    std::vector<float> marks = {-60, -40, -20, -10, -6, -3, 0, 3, 6};

    for (float db : marks)
    {
        if (db < minDb || db > maxDb)
            continue;

        float normalized = (db - minDb) / (maxDb - minDb);
        juce::String label = (db >= 0 ? "+" : "") + juce::String(static_cast<int>(db));

        if (orientation == Orientation::Horizontal)
        {
            float x = bounds.getX() + bounds.getWidth() * normalized;
            g.drawText(label, static_cast<int>(x) - 10, static_cast<int>(bounds.getBottom()) - 12,
                       20, 12, juce::Justification::centred);
        }
        else
        {
            float y = bounds.getBottom() - bounds.getHeight() * normalized;
            g.drawText(label, static_cast<int>(bounds.getRight()) - 25,
                       static_cast<int>(y) - 6, 20, 12, juce::Justification::centredRight);
        }
    }
}

juce::Colour VUMeter::getLevelColour(float position) const
{
    if (position < 0.6f)
        return lowColour;
    else if (position < 0.85f)
        return midColour;
    else
        return highColour;
}

//=============================================================================
// StereoVUMeter Implementation
//=============================================================================

StereoVUMeter::StereoVUMeter()
{
    leftMeter = std::make_unique<VUMeter>();
    rightMeter = std::make_unique<VUMeter>();

    addAndMakeVisible(leftMeter.get());
    addAndMakeVisible(rightMeter.get());
}

void StereoVUMeter::paint(juce::Graphics& g)
{
    // Labels
    g.setColour(Colors::textDim);
    g.setFont(10.0f);

    if (leftMeter->getWidth() < leftMeter->getHeight())
    {
        // Vertical orientation
        g.drawText("L", leftMeter->getX(), getHeight() - 12,
                   leftMeter->getWidth(), 12, juce::Justification::centred);
        g.drawText("R", rightMeter->getX(), getHeight() - 12,
                   rightMeter->getWidth(), 12, juce::Justification::centred);
    }
}

void StereoVUMeter::resized()
{
    auto bounds = getLocalBounds();

    bool vertical = bounds.getHeight() > bounds.getWidth();

    if (vertical)
    {
        bounds.removeFromBottom(15); // Space for labels
        int meterWidth = (bounds.getWidth() - 4) / 2;
        leftMeter->setBounds(bounds.removeFromLeft(meterWidth));
        bounds.removeFromLeft(4);
        rightMeter->setBounds(bounds);
    }
    else
    {
        int meterHeight = (bounds.getHeight() - 4) / 2;
        leftMeter->setBounds(bounds.removeFromTop(meterHeight));
        bounds.removeFromTop(4);
        rightMeter->setBounds(bounds);
    }
}

void StereoVUMeter::setLevels(float leftPeak, float rightPeak)
{
    leftMeter->setLevel(leftPeak);
    rightMeter->setLevel(rightPeak);
}

void StereoVUMeter::setLevels(float leftPeak, float rightPeak, float leftRMS, float rightRMS)
{
    leftMeter->setLevels(leftPeak, leftRMS);
    rightMeter->setLevels(rightPeak, rightRMS);
}

void StereoVUMeter::setOrientation(VUMeter::Orientation orientation)
{
    leftMeter->setOrientation(orientation);
    rightMeter->setOrientation(orientation);
    resized();
}

} // namespace ARC

