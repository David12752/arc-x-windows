/*
 * ============================================================================
 * ARC System Audio Host - Audio Processor Implementation
 * ============================================================================
 */

#include "AudioProcessor.h"
#include "../Utils/Logger.h"
#include <cmath>

namespace ARC
{

//=============================================================================
// BiquadFilter Implementation
//=============================================================================

void BiquadFilter::setCoefficients(Type type, double sampleRate, double frequency,
                                   double q, double gainDb)
{
    double w0 = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
    double cosw0 = std::cos(w0);
    double sinw0 = std::sin(w0);
    double alpha = sinw0 / (2.0 * q);

    double a0 = 1.0;

    switch (type)
    {
        case Type::LowPass:
            b0 = (1.0 - cosw0) / 2.0;
            b1 = 1.0 - cosw0;
            b2 = (1.0 - cosw0) / 2.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosw0;
            a2 = 1.0 - alpha;
            break;

        case Type::HighPass:
            b0 = (1.0 + cosw0) / 2.0;
            b1 = -(1.0 + cosw0);
            b2 = (1.0 + cosw0) / 2.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosw0;
            a2 = 1.0 - alpha;
            break;

        case Type::BandPass:
            b0 = alpha;
            b1 = 0.0;
            b2 = -alpha;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosw0;
            a2 = 1.0 - alpha;
            break;

        case Type::Notch:
            b0 = 1.0;
            b1 = -2.0 * cosw0;
            b2 = 1.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosw0;
            a2 = 1.0 - alpha;
            break;

        case Type::Peak:
        {
            double A = std::pow(10.0, gainDb / 40.0);
            b0 = 1.0 + alpha * A;
            b1 = -2.0 * cosw0;
            b2 = 1.0 - alpha * A;
            a0 = 1.0 + alpha / A;
            a1 = -2.0 * cosw0;
            a2 = 1.0 - alpha / A;
            break;
        }

        case Type::LowShelf:
        {
            double A = std::pow(10.0, gainDb / 40.0);
            double sqrtA = std::sqrt(A);
            b0 = A * ((A + 1.0) - (A - 1.0) * cosw0 + 2.0 * sqrtA * alpha);
            b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cosw0);
            b2 = A * ((A + 1.0) - (A - 1.0) * cosw0 - 2.0 * sqrtA * alpha);
            a0 = (A + 1.0) + (A - 1.0) * cosw0 + 2.0 * sqrtA * alpha;
            a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cosw0);
            a2 = (A + 1.0) + (A - 1.0) * cosw0 - 2.0 * sqrtA * alpha;
            break;
        }

        case Type::HighShelf:
        {
            double A = std::pow(10.0, gainDb / 40.0);
            double sqrtA = std::sqrt(A);
            b0 = A * ((A + 1.0) + (A - 1.0) * cosw0 + 2.0 * sqrtA * alpha);
            b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosw0);
            b2 = A * ((A + 1.0) + (A - 1.0) * cosw0 - 2.0 * sqrtA * alpha);
            a0 = (A + 1.0) - (A - 1.0) * cosw0 + 2.0 * sqrtA * alpha;
            a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cosw0);
            a2 = (A + 1.0) - (A - 1.0) * cosw0 - 2.0 * sqrtA * alpha;
            break;
        }
    }

    // Normalize coefficients
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a1 /= a0;
    a2 /= a0;
}

void BiquadFilter::reset()
{
    z1 = 0.0;
    z2 = 0.0;
}

float BiquadFilter::process(float input)
{
    double output = b0 * input + z1;
    z1 = b1 * input - a1 * output + z2;
    z2 = b2 * input - a2 * output;
    return static_cast<float>(output);
}

void BiquadFilter::processBlock(float* data, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        data[i] = process(data[i]);
    }
}

//=============================================================================
// LookAheadLimiter Implementation  — True Forward-Scanning Look-Ahead
//=============================================================================

LookAheadLimiter::LookAheadLimiter()
{
}

void LookAheadLimiter::prepare(double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate;

    // Pre-allocate per-channel rings.  We need room for:
    //   • LOOKAHEAD_SAMPLES of look-ahead content
    //   • One full block of newly-arriving samples
    // Keeping RING_CAPACITY fixed avoids any runtime heap allocation.
    const int ch = 2; // stereo
    numChannels = ch;
    ring.assign(ch, std::vector<float>(RING_CAPACITY, 0.0f));
    writeHead = 0;

    // Pre-allocate the per-block gain envelope (avoids heap in process())
    gainEnvelope.assign(static_cast<size_t>(samplesPerBlock), 1.0f);

    reset();
}

void LookAheadLimiter::setThreshold(float thresholdDb)
{
    threshold = juce::Decibels::decibelsToGain(thresholdDb);
}

void LookAheadLimiter::setRelease(float releaseMs)
{
    releaseCoeff = std::exp(-1.0f / (releaseMs * 0.001f * static_cast<float>(sampleRate)));
}

void LookAheadLimiter::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples  = buffer.getNumSamples();
    const int numCh       = juce::jmin(buffer.getNumChannels(), numChannels);

    // Grow gain envelope if block size increased (rare, but safe)
    if (static_cast<int>(gainEnvelope.size()) < numSamples)
        gainEnvelope.assign(static_cast<size_t>(numSamples), 1.0f);

    // ----------------------------------------------------------------
    // Pass 1: Write all incoming samples into the delay ring.
    //         The ring is indexed modulo RING_CAPACITY.
    // ----------------------------------------------------------------
    for (int s = 0; s < numSamples; ++s)
    {
        const int pos = (writeHead + s) % RING_CAPACITY;
        for (int ch = 0; ch < numCh; ++ch)
            ring[ch][pos] = buffer.getSample(ch, s);
    }

    // ----------------------------------------------------------------
    // Pass 2: For each *output* sample (= the delayed sample at
    //         readHead), scan LOOKAHEAD_SAMPLES positions *ahead* in
    //         the ring to find the worst-case future peak, then build
    //         the gain envelope.
    //
    //   readHead = writeHead - LOOKAHEAD_SAMPLES
    //   scanStart = readHead + 1   (i.e. writeHead - LOOKAHEAD + 1)
    //   scanEnd   = readHead + LOOKAHEAD_SAMPLES (i.e. writeHead)
    //
    //   This means we always look at samples that have already arrived
    //   but have not yet been emitted — true look-ahead.
    // ----------------------------------------------------------------
    const int ringSize = RING_CAPACITY;
    const int readBase = (writeHead - LOOKAHEAD_SAMPLES + ringSize) % ringSize;

    for (int s = 0; s < numSamples; ++s)
    {
        const int readPos = (readBase + s) % ringSize;

        // Scan the LOOKAHEAD window ahead of this read position
        float futurePeak = 0.0f;
        for (int look = 1; look <= LOOKAHEAD_SAMPLES; ++look)
        {
            const int scanPos = (readPos + look) % ringSize;
            for (int ch = 0; ch < numCh; ++ch)
                futurePeak = std::max(futurePeak, std::abs(ring[ch][scanPos]));
        }

        // Compute target gain from future peak
        const float targetGain = (futurePeak > threshold)
                                    ? (threshold / futurePeak)
                                    : 1.0f;

        // Attack: instantaneous (the look-ahead window has already
        //         absorbed the transient — we know it's coming).
        // Release: exponential smoothing.
        if (targetGain < currentGain)
            currentGain = targetGain;
        else
            currentGain = currentGain * releaseCoeff
                        + targetGain  * (1.0f - releaseCoeff);

        gainEnvelope[s] = currentGain;
    }

    // ----------------------------------------------------------------
    // Pass 3: Apply the envelope to the *delayed* samples and write
    //         them back to the output buffer.
    // ----------------------------------------------------------------
    for (int s = 0; s < numSamples; ++s)
    {
        const int readPos = (readBase + s) % ringSize;
        const float g     = gainEnvelope[s];

        for (int ch = 0; ch < numCh; ++ch)
            buffer.setSample(ch, s, ring[ch][readPos] * g);
    }

    // Advance write head by one full block
    writeHead = (writeHead + numSamples) % ringSize;

    // Expose worst-case gain reduction of this block for metering
    float minGain = 1.0f;
    for (int s = 0; s < numSamples; ++s)
        minGain = std::min(minGain, gainEnvelope[s]);
    gainReduction.store(1.0f - minGain, std::memory_order_relaxed);
}

void LookAheadLimiter::reset()
{
    currentGain = 1.0f;
    gainReduction.store(0.0f, std::memory_order_relaxed);
    writeHead = 0;
    for (auto& ch : ring)
        std::fill(ch.begin(), ch.end(), 0.0f);
}

//=============================================================================
// AudioProcessor Implementation
//=============================================================================

AudioProcessor::AudioProcessor()
    : fft(static_cast<int>(std::log2(SpectrumData::FFT_SIZE))),
      window(SpectrumData::FFT_SIZE, juce::dsp::WindowingFunction<float>::hann)
{
    Logger::log("AudioProcessor created");
}

AudioProcessor::~AudioProcessor()
{
    Logger::log("AudioProcessor destroyed");
}

//=============================================================================
void AudioProcessor::prepare(double sampleRate, int samplesPerBlock, int channels)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    numChannels = channels;

    // Initialize smoothed values
    inputGain.reset(sampleRate, 0.02);
    inputGain.setCurrentAndTargetValue(1.0f);
    outputGain.reset(sampleRate, 0.02);
    outputGain.setCurrentAndTargetValue(1.0f);

    // Initialize limiter
    limiter.prepare(sampleRate, samplesPerBlock);
    limiter.setThreshold(-0.3f);
    limiter.setRelease(100.0f);

    // Initialize DC filters (high-pass at 20Hz)
    for (auto& filter : dcFilters)
    {
        filter.setCoefficients(BiquadFilter::Type::HighPass, sampleRate, 20.0, 0.707);
        filter.reset();
    }

    // Clear FFT data
    fftData.fill(0.0f);
    fftWritePos = 0;

    // Reset levels
    resetLevelData();

    prepared = true;
    Logger::log("AudioProcessor prepared: " + juce::String(sampleRate) + " Hz, " +
                juce::String(samplesPerBlock) + " samples, " +
                juce::String(channels) + " channels");
}

void AudioProcessor::process(juce::AudioBuffer<float>& buffer)
{
    if (!prepared)
        return;

    const int numSamples = buffer.getNumSamples();

    // Apply input gain.
    // Advance smoother once per sample; apply the same scalar to all channels
    // so L and R receive an identical gain curve (no stereo image drift).
    if (inputGain.isSmoothing() || std::abs(inputGain.getTargetValue() - 1.0f) > 0.0001f)
    {
        const int numCh = buffer.getNumChannels();
        for (int s = 0; s < numSamples; ++s)
        {
            const float g = inputGain.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer(ch)[s] *= g;
        }
    }

    // Apply DC filter
    if (dcFilterEnabled.load())
    {
        applyDCFilter(buffer);
    }

    // Update spectrum data
    if (spectrumEnabled.load())
    {
        updateSpectrum(buffer);
    }

    // Apply limiter
    if (limiterEnabled.load())
    {
        limiter.process(buffer);
    }

    // Apply output gain (same channel-inner pattern as input gain above).
    if (outputGain.isSmoothing() || std::abs(outputGain.getTargetValue() - 1.0f) > 0.0001f)
    {
        const int numCh = buffer.getNumChannels();
        for (int s = 0; s < numSamples; ++s)
        {
            const float g = outputGain.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer(ch)[s] *= g;
        }
    }

    // Update level meters
    updateLevels(buffer);
}

void AudioProcessor::reset()
{
    // Reset limiter
    limiter.reset();

    // Reset DC filters
    for (auto& filter : dcFilters)
    {
        filter.reset();
    }

    // Reset spectrum
    fftData.fill(0.0f);
    fftWritePos = 0;
    spectrumData.reset();

    // Reset levels
    resetLevelData();

    Logger::log("AudioProcessor reset");
}

//=============================================================================
void AudioProcessor::setInputGain(float gainDb)
{
    inputGain.setTargetValue(juce::Decibels::decibelsToGain(gainDb));
}

float AudioProcessor::getInputGain() const
{
    return juce::Decibels::gainToDecibels(inputGain.getTargetValue());
}

void AudioProcessor::setOutputGain(float gainDb)
{
    outputGain.setTargetValue(juce::Decibels::decibelsToGain(gainDb));
}

float AudioProcessor::getOutputGain() const
{
    return juce::Decibels::gainToDecibels(outputGain.getTargetValue());
}

//=============================================================================
void AudioProcessor::setLimiterEnabled(bool enabled)
{
    limiterEnabled.store(enabled);
}

bool AudioProcessor::isLimiterEnabled() const
{
    return limiterEnabled.load();
}

void AudioProcessor::setLimiterThreshold(float thresholdDb)
{
    limiter.setThreshold(thresholdDb);
}

float AudioProcessor::getLimiterGainReduction() const
{
    return limiter.getGainReduction();
}

//=============================================================================
void AudioProcessor::setDCFilterEnabled(bool enabled)
{
    dcFilterEnabled.store(enabled);
}

bool AudioProcessor::isDCFilterEnabled() const
{
    return dcFilterEnabled.load();
}

//=============================================================================
LevelData AudioProcessor::getLevelData() const
{
    std::lock_guard<std::mutex> lock(levelMutex);
    return levels;
}

void AudioProcessor::resetLevelData()
{
    std::lock_guard<std::mutex> lock(levelMutex);
    levels = LevelData{};
}

//=============================================================================
bool AudioProcessor::isSpectrumReady() const
{
    return spectrumReady.load();
}

SpectrumData AudioProcessor::getSpectrumData()
{
    std::lock_guard<std::mutex> lock(spectrumMutex);
    spectrumReady.store(false);
    return spectrumData;
}

void AudioProcessor::enableSpectrum(bool enable)
{
    spectrumEnabled.store(enable);
}

//=============================================================================
int AudioProcessor::getLatencySamples() const
{
    // Look-ahead limiter latency
    return limiterEnabled.load() ? LookAheadLimiter::getLookaheadSamples() : 0;
}

//=============================================================================
void AudioProcessor::updateLevels(const juce::AudioBuffer<float>& buffer)
{
    std::lock_guard<std::mutex> lock(levelMutex);

    const int numSamples = buffer.getNumSamples();

    // Decay existing levels
    levels.decay(0.9995f, 0.99f, peakHoldTime);

    // Calculate new levels
    if (buffer.getNumChannels() >= 1)
    {
        const float* dataL = buffer.getReadPointer(0);
        float peakL = 0.0f;
        float sumL = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float absVal = std::abs(dataL[i]);
            peakL = juce::jmax(peakL, absVal);
            sumL += dataL[i] * dataL[i];
        }

        levels.peakL = juce::jmax(levels.peakL, peakL);
        levels.rmsL = juce::jmax(levels.rmsL, std::sqrt(sumL / numSamples));

        if (peakL > levels.peakHoldL)
        {
            levels.peakHoldL = peakL;
            levels.peakHoldSamplesL = peakHoldTime;
        }
    }

    if (buffer.getNumChannels() >= 2)
    {
        const float* dataR = buffer.getReadPointer(1);
        float peakR = 0.0f;
        float sumR = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float absVal = std::abs(dataR[i]);
            peakR = juce::jmax(peakR, absVal);
            sumR += dataR[i] * dataR[i];
        }

        levels.peakR = juce::jmax(levels.peakR, peakR);
        levels.rmsR = juce::jmax(levels.rmsR, std::sqrt(sumR / numSamples));

        if (peakR > levels.peakHoldR)
        {
            levels.peakHoldR = peakR;
            levels.peakHoldSamplesR = peakHoldTime;
        }
    }
}

void AudioProcessor::updateSpectrum(const juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();

    // Mix to mono and add to FFT buffer
    for (int i = 0; i < numSamples; ++i)
    {
        float sample = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            sample += buffer.getSample(ch, i);
        }
        sample /= buffer.getNumChannels();

        fftData[fftWritePos++] = sample;

        if (fftWritePos >= SpectrumData::FFT_SIZE)
        {
            // Apply window
            window.multiplyWithWindowingTable(fftData.data(), SpectrumData::FFT_SIZE);

            // Perform FFT
            fft.performFrequencyOnlyForwardTransform(fftData.data());

            // Update spectrum data
            {
                std::lock_guard<std::mutex> lock(spectrumMutex);

                for (int bin = 0; bin < SpectrumData::NUM_BINS; ++bin)
                {
                    float magnitude = fftData[bin] / SpectrumData::FFT_SIZE;
                    spectrumData.magnitudes[bin] = magnitude;

                    // Smooth the spectrum
                    spectrumData.smoothedMagnitudes[bin] =
                        spectrumData.smoothedMagnitudes[bin] * spectrumSmoothing +
                        magnitude * (1.0f - spectrumSmoothing);
                }
            }

            spectrumReady.store(true);
            fftWritePos = 0;
        }
    }
}

void AudioProcessor::applyDCFilter(juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < juce::jmin(2, buffer.getNumChannels()); ++ch)
    {
        dcFilters[ch].processBlock(buffer.getWritePointer(ch), buffer.getNumSamples());
    }
}

} // namespace ARC

