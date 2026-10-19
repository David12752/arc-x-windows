/*
 * ============================================================================
 * ARC System Audio Host - Spectrum Analyzer Implementation
 * ============================================================================
 */

#include "SpectrumAnalyzer.h"
#include "LookAndFeel.h"
#include <cmath>

namespace ARC
{

//=============================================================================
SpectrumAnalyzer::SpectrumAnalyzer()
    : fft(FFT_ORDER),
      window(FFT_SIZE, juce::dsp::WindowingFunction<float>::hann),
      lowColour(Colors::accent),
      highColour(Colors::accentSecondary)
{
    fftData.fill(0.0f);
    magnitudes.fill(0.0f);
    smoothedMagnitudes.fill(0.0f);

    startTimerHz(30);
}

SpectrumAnalyzer::~SpectrumAnalyzer()
{
    stopTimer();
}

void SpectrumAnalyzer::paint(juce::Graphics& g)
{
    drawBackground(g);

    if (showGrid)
    {
        drawGrid(g);
    }

    drawSpectrum(g);

    if (showLabels)
    {
        drawLabels(g);
    }
}

void SpectrumAnalyzer::resized()
{
    // Rebuild spectrum path on resize
}

void SpectrumAnalyzer::timerCallback()
{
    if (fftReady.load())
    {
        processFFT();
        fftReady.store(false);
    }

    repaint();
}

void SpectrumAnalyzer::pushSamples(const float* samples, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        fftData[fftWriteIndex++] = samples[i];

        if (fftWriteIndex >= FFT_SIZE)
        {
            fftWriteIndex = 0;
            fftReady.store(true);
        }
    }
}

void SpectrumAnalyzer::setSpectrumData(const SpectrumData& data)
{
    for (int i = 0; i < juce::jmin(NUM_BINS, static_cast<int>(data.magnitudes.size())); ++i)
    {
        magnitudes[i] = data.magnitudes[i];
    }
}

void SpectrumAnalyzer::setDisplayMode(DisplayMode mode)
{
    displayMode = mode;
    repaint();
}

void SpectrumAnalyzer::setScaleMode(ScaleMode mode)
{
    scaleMode = mode;
    repaint();
}

void SpectrumAnalyzer::setFrequencyRange(float minHz, float maxHz)
{
    minFrequency = minHz;
    maxFrequency = maxHz;
    repaint();
}

void SpectrumAnalyzer::setDecibelRange(float minDb, float maxDb)
{
    minDecibels = minDb;
    maxDecibels = maxDb;
    repaint();
}

void SpectrumAnalyzer::setSmoothingFactor(float factor)
{
    smoothingFactor = juce::jlimit(0.0f, 0.99f, factor);
}

void SpectrumAnalyzer::setShowGrid(bool show)
{
    showGrid = show;
    repaint();
}

void SpectrumAnalyzer::setShowLabels(bool show)
{
    showLabels = show;
    repaint();
}

void SpectrumAnalyzer::setGradientColors(const juce::Colour& low, const juce::Colour& high)
{
    lowColour = low;
    highColour = high;
    repaint();
}

void SpectrumAnalyzer::processFFT()
{
    // Apply window function
    window.multiplyWithWindowingTable(fftData.data(), FFT_SIZE);

    // Perform FFT
    fft.performFrequencyOnlyForwardTransform(fftData.data());

    // Convert to magnitudes
    for (int i = 0; i < NUM_BINS; ++i)
    {
        float magnitude = fftData[i] / FFT_SIZE;
        magnitude = juce::jmax(magnitude, 0.00001f); // Avoid log(0)

        // Smooth the magnitude
        smoothedMagnitudes[i] = smoothedMagnitudes[i] * smoothingFactor +
                                 magnitude * (1.0f - smoothingFactor);
    }
}

void SpectrumAnalyzer::drawBackground(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background
    g.setColour(Colors::backgroundPanel);
    g.fillRoundedRectangle(bounds, 8.0f);

    // Border
    g.setColour(Colors::border);
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
}

void SpectrumAnalyzer::drawGrid(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(30, 20);

    g.setColour(Colors::border.withAlpha(0.5f));

    // Frequency lines (logarithmic)
    std::vector<float> frequencies = {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};

    for (float freq : frequencies)
    {
        if (freq >= minFrequency && freq <= maxFrequency)
        {
            float x = frequencyToX(freq);
            g.drawLine(x, bounds.getY(), x, bounds.getBottom(), 0.5f);
        }
    }

    // Decibel lines
    for (float db = minDecibels; db <= maxDecibels; db += 12.0f)
    {
        float y = magnitudeToY(juce::Decibels::decibelsToGain(db));
        g.drawLine(bounds.getX(), y, bounds.getRight(), y, 0.5f);
    }
}

void SpectrumAnalyzer::drawSpectrum(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(30, 20);

    if (bounds.isEmpty())
        return;

    spectrumPath.clear();

    bool pathStarted = false;
    float lastX = 0;
    float lastY = bounds.getBottom();

    for (int bin = 1; bin < NUM_BINS; ++bin)
    {
        float frequency = bin * sampleRate / FFT_SIZE;

        if (frequency < minFrequency || frequency > maxFrequency)
            continue;

        float x = frequencyToX(frequency);
        float y = magnitudeToY(smoothedMagnitudes[bin]);

        y = juce::jlimit(bounds.getY(), bounds.getBottom(), y);

        if (!pathStarted)
        {
            spectrumPath.startNewSubPath(x, bounds.getBottom());
            spectrumPath.lineTo(x, y);
            pathStarted = true;
        }
        else
        {
            spectrumPath.lineTo(x, y);
        }

        lastX = x;
        lastY = y;
    }

    if (pathStarted)
    {
        if (displayMode == DisplayMode::FilledLine)
        {
            spectrumPath.lineTo(lastX, bounds.getBottom());
            spectrumPath.closeSubPath();

            juce::ColourGradient gradient(lowColour.withAlpha(0.6f), bounds.getX(), bounds.getBottom(),
                                          highColour.withAlpha(0.6f), bounds.getRight(), bounds.getY(),
                                          false);
            g.setGradientFill(gradient);
            g.fillPath(spectrumPath);

            // Draw line on top
            g.setColour(lowColour);
            g.strokePath(spectrumPath, juce::PathStrokeType(1.5f));
        }
        else if (displayMode == DisplayMode::Line)
        {
            g.setColour(lowColour);
            g.strokePath(spectrumPath, juce::PathStrokeType(2.0f));
        }
        else // Bars
        {
            // Draw as individual bars
            float barWidth = bounds.getWidth() / 64.0f;
            for (int i = 0; i < 64; ++i)
            {
                int bin = 1 + (i * NUM_BINS / 64);
                if (bin >= NUM_BINS) bin = NUM_BINS - 1;

                float frequency = bin * sampleRate / FFT_SIZE;
                float x = frequencyToX(frequency);
                float y = magnitudeToY(smoothedMagnitudes[bin]);

                float height = bounds.getBottom() - y;

                juce::ColourGradient gradient(lowColour, 0, bounds.getBottom(),
                                              highColour, 0, bounds.getY(), false);
                g.setGradientFill(gradient);
                g.fillRect(x - barWidth / 2, y, barWidth - 1, height);
            }
        }
    }
}

void SpectrumAnalyzer::drawLabels(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(30, 20);

    g.setColour(Colors::textDim);
    g.setFont(10.0f);

    // Frequency labels
    std::vector<std::pair<float, juce::String>> freqLabels = {
        {20, "20"}, {50, "50"}, {100, "100"}, {200, "200"}, {500, "500"},
        {1000, "1k"}, {2000, "2k"}, {5000, "5k"}, {10000, "10k"}, {20000, "20k"}
    };

    for (const auto& [freq, label] : freqLabels)
    {
        if (freq >= minFrequency && freq <= maxFrequency)
        {
            float x = frequencyToX(freq);
            g.drawText(label, static_cast<int>(x) - 15,
                       static_cast<int>(bounds.getBottom()) + 2, 30, 15,
                       juce::Justification::centred);
        }
    }

    // Decibel labels
    for (float db = minDecibels + 12; db <= maxDecibels; db += 12.0f)
    {
        float y = magnitudeToY(juce::Decibels::decibelsToGain(db));
        juce::String label = (db >= 0 ? "+" : "") + juce::String(static_cast<int>(db));
        g.drawText(label, 2, static_cast<int>(y) - 6, 25, 12,
                   juce::Justification::centredRight);
    }
}

float SpectrumAnalyzer::frequencyToX(float frequency) const
{
    auto bounds = getLocalBounds().toFloat().reduced(30, 20);

    if (scaleMode == ScaleMode::Logarithmic)
    {
        float logMin = std::log10(minFrequency);
        float logMax = std::log10(maxFrequency);
        float logFreq = std::log10(frequency);

        return bounds.getX() + (logFreq - logMin) / (logMax - logMin) * bounds.getWidth();
    }
    else
    {
        return bounds.getX() + (frequency - minFrequency) /
               (maxFrequency - minFrequency) * bounds.getWidth();
    }
}

float SpectrumAnalyzer::magnitudeToY(float magnitude) const
{
    auto bounds = getLocalBounds().toFloat().reduced(30, 20);

    float db = juce::Decibels::gainToDecibels(magnitude, minDecibels);
    float normalized = (db - minDecibels) / (maxDecibels - minDecibels);
    normalized = juce::jlimit(0.0f, 1.0f, normalized);

    return bounds.getBottom() - normalized * bounds.getHeight();
}

//=============================================================================
// FrequencyResponseDisplay Implementation
//=============================================================================

FrequencyResponseDisplay::FrequencyResponseDisplay()
{
}

void FrequencyResponseDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(30, 20);

    // Background
    g.setColour(Colors::backgroundPanel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);

    // Grid
    g.setColour(Colors::border.withAlpha(0.5f));

    // 0 dB reference line
    if (showReference)
    {
        float zeroY = bounds.getCentreY();
        g.setColour(Colors::textDim);
        g.drawLine(bounds.getX(), zeroY, bounds.getRight(), zeroY, 1.0f);
    }

    // Draw curve
    if (!responseCurve.empty())
    {
        curvePath.clear();
        bool started = false;

        for (const auto& [freq, db] : responseCurve)
        {
            float logMin = std::log10(minFrequency);
            float logMax = std::log10(maxFrequency);
            float logFreq = std::log10(freq);

            float x = bounds.getX() + (logFreq - logMin) / (logMax - logMin) * bounds.getWidth();
            float normalized = (db - minDecibels) / (maxDecibels - minDecibels);
            float y = bounds.getBottom() - normalized * bounds.getHeight();

            if (!started)
            {
                curvePath.startNewSubPath(x, y);
                started = true;
            }
            else
            {
                curvePath.lineTo(x, y);
            }
        }

        g.setColour(Colors::accent);
        g.strokePath(curvePath, juce::PathStrokeType(2.0f));
    }

    // Labels
    g.setColour(Colors::textDim);
    g.setFont(10.0f);
    g.drawText("Frequency Response", bounds.getX(), 2, 150, 15, juce::Justification::centredLeft);
}

void FrequencyResponseDisplay::resized()
{
}

void FrequencyResponseDisplay::setResponseCurve(const std::vector<std::pair<float, float>>& curve)
{
    responseCurve = curve;
    repaint();
}

void FrequencyResponseDisplay::clearResponseCurve()
{
    responseCurve.clear();
    repaint();
}

void FrequencyResponseDisplay::setFrequencyRange(float minHz, float maxHz)
{
    minFrequency = minHz;
    maxFrequency = maxHz;
    repaint();
}

void FrequencyResponseDisplay::setDecibelRange(float minDb, float maxDb)
{
    minDecibels = minDb;
    maxDecibels = maxDb;
    repaint();
}

void FrequencyResponseDisplay::setShowReference(bool show)
{
    showReference = show;
    repaint();
}

} // namespace ARC

