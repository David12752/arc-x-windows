/*
 * ============================================================================
 * ARC System Audio Host - Real-Time Audio Pipeline Implementation
 * ============================================================================
 */

#include "AudioPipeline.h"
#include "../Utils/RTGuard.h"
#include <cmath>
#include <algorithm>
#include <variant>
#include <cstring>

namespace ARC
{

//=============================================================================
AudioPipeline::AudioPipeline(VSTChain&        vstChain,
                             AudioProcessor&  audioProcessor,
                             CommandQueue&    commandQueue,
                             MetricsQueue&    metricsQueue)
    : vstChain_      (vstChain)
    , audioProcessor_(audioProcessor)
    , commandQueue_  (commandQueue)
    , metricsQueue_  (metricsQueue)
{
}

//=============================================================================
void AudioPipeline::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sampleRate_     = sampleRate;
    samplesPerBlock_ = samplesPerBlock;

    // Publish metrics at ~60Hz
    // e.g. @ 48kHz / 256 samples = 187.5 blocks/sec → every 3 blocks ≈ 62Hz
    const double blocksPerSec = sampleRate / static_cast<double>(samplesPerBlock);
    metricsPublishInterval_   = std::max(1, static_cast<int>(blocksPerSec / 60.0));

    vstChain_.prepareToPlay(sampleRate, samplesPerBlock);
    audioProcessor_.prepareToPlay(sampleRate, samplesPerBlock);

    running_.store(true, std::memory_order_release);
}

void AudioPipeline::releaseResources()
{
    running_.store(false, std::memory_order_release);
    vstChain_.releaseResources();
    audioProcessor_.releaseResources();
}

//=============================================================================
void AudioPipeline::processBlock(juce::AudioBuffer<float>& buffer,
                                  juce::MidiBuffer& /*midiBuffer*/)
{
    ARC_ASSERT_RT_SAFE();

    // Capture block start time for CPU measurement
    const double t0 = juce::Time::getMillisecondCounterHiRes();

    // -----------------------------------------------------------------------
    // 1. Drain command queue (Non-RT→RT) — wait-free, zero allocation
    // -----------------------------------------------------------------------
    drainCommandQueue();

    // -----------------------------------------------------------------------
    // 2. If a loopback queue is attached, pull the captured AudioBlock and
    //    copy it into `buffer` so downstream processing sees real audio.
    //    If the queue is empty (underrun), fill with silence and xrun.
    // -----------------------------------------------------------------------
    if (loopbackQueue_ != nullptr)
    {
        AudioBlock block;
        if (loopbackQueue_->try_dequeue(block))
        {
            const int nCh = juce::jmin(buffer.getNumChannels(), block.numChannels);
            const int nSamples = juce::jmin(buffer.getNumSamples(), block.numSamples);

            for (int ch = 0; ch < nCh; ++ch)
            {
                float* dest = buffer.getWritePointer(ch);
                std::memcpy(dest, block.data[ch],
                            static_cast<std::size_t>(nSamples) * sizeof(float));
                // Zero any extra samples if block is shorter than device block
                if (nSamples < buffer.getNumSamples())
                    std::memset(dest + nSamples, 0,
                                static_cast<std::size_t>(buffer.getNumSamples() - nSamples)
                                * sizeof(float));
            }
            // Fill remaining channels (if buffer is wider) with silence
            for (int ch = nCh; ch < buffer.getNumChannels(); ++ch)
                buffer.clear(ch, 0, buffer.getNumSamples());
        }
        else
        {
            // Loopback queue empty — underrun: output silence
            buffer.clear();
            xrunCount_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // -----------------------------------------------------------------------
    // 3. Check master bypass
    // -----------------------------------------------------------------------
    if (masterBypassed_.load(std::memory_order_relaxed))
    {
        captureMetrics(buffer, t0);
        return;
    }

    // -----------------------------------------------------------------------
    // 4. VST chain processing
    // -----------------------------------------------------------------------
    vstChain_.processBlock(buffer, dummyMidi_);

    // -----------------------------------------------------------------------
    // 5. Post-chain DSP: limiter + output gain (AudioProcessor)
    // -----------------------------------------------------------------------
    audioProcessor_.processBlock(buffer, dummyMidi_);

    // -----------------------------------------------------------------------
    // 6. Capture metrics, optionally enqueue for 60Hz publisher
    // -----------------------------------------------------------------------
    captureMetrics(buffer, t0);
}

//=============================================================================
void AudioPipeline::drainCommandQueue() noexcept
{
    EngineCommand cmd;
    while (commandQueue_.try_dequeue(cmd))
    {
        std::visit([this](auto&& c) { this->apply(c); }, cmd);
    }
}

//=============================================================================
void AudioPipeline::captureMetrics(const juce::AudioBuffer<float>& buffer,
                                    double blockStartTimeMs) noexcept
{
    const int numCh      = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Peak and RMS (RT-local, no allocation)
    float rawPeakL = 0.0f, rawPeakR = 0.0f;
    float sumSqL   = 0.0f, sumSqR   = 0.0f;

    if (numCh > 0)
    {
        const float* chL = buffer.getReadPointer(0);
        for (int i = 0; i < numSamples; ++i)
        {
            const float s   = std::abs(chL[i]);
            if (s > rawPeakL) rawPeakL = s;
            sumSqL += s * s;
        }
    }

    if (numCh > 1)
    {
        const float* chR = buffer.getReadPointer(1);
        for (int i = 0; i < numSamples; ++i)
        {
            const float s   = std::abs(chR[i]);
            if (s > rawPeakR) rawPeakR = s;
            sumSqR += s * s;
        }
    }
    else
    {
        rawPeakR = rawPeakL;
        sumSqR   = sumSqL;
    }

    // RMS (per-block, not windowed — UI can smooth if desired)
    const float rmsL = (numSamples > 0)
        ? std::sqrt(sumSqL / static_cast<float>(numSamples))
        : 0.0f;
    const float rmsR = (numSamples > 0)
        ? std::sqrt(sumSqR / static_cast<float>(numSamples))
        : 0.0f;

    // Peak hold decay
    peakHoldL_ = std::max(rawPeakL, peakHoldL_ * peakHoldDecay_);
    peakHoldR_ = std::max(rawPeakR, peakHoldR_ * peakHoldDecay_);

    // Expose peak via atomics for external polling
    peakL_.store(rawPeakL, std::memory_order_relaxed);
    peakR_.store(rawPeakR, std::memory_order_relaxed);

    // CPU % estimate for this block
    const double t1      = juce::Time::getMillisecondCounterHiRes();
    const double blockMs = (sampleRate_ > 0.0)
        ? (static_cast<double>(numSamples) / sampleRate_) * 1000.0
        : 0.0;
    const float  cpuPct  = (blockMs > 0.0)
        ? static_cast<float>((t1 - blockStartTimeMs) / blockMs * 100.0)
        : 0.0f;

    cpuPercent_.store(cpuPct, std::memory_order_relaxed);

    // -----------------------------------------------------------------------
    // Publish to MetricsQueue at ~60Hz
    // -----------------------------------------------------------------------
    ++metricsTick_;
    if (metricsTick_ < metricsPublishInterval_)
        return;

    metricsTick_ = 0;

    MetricsFrame f{};
    f.sequenceNumber     = ++frameCounter_;
    f.peakL              = rawPeakL;
    f.peakR              = rawPeakR;
    f.rmsL               = rmsL;
    f.rmsR               = rmsR;
    f.peakHoldL          = peakHoldL_;
    f.peakHoldR          = peakHoldR_;
    f.cpuPercent         = cpuPct;
    f.engineRunning      = 1;
    f.masterBypassed     = masterBypassed_.load(std::memory_order_relaxed) ? 1 : 0;
    f.totalLatencySamples = vstChain_.getTotalLatencySamples();
    f.totalLatencyMs     = (sampleRate_ > 0.0)
        ? static_cast<float>(static_cast<double>(f.totalLatencySamples) / sampleRate_ * 1000.0)
        : 0.0f;
    f.xrunCount          = xrunCount_.load(std::memory_order_relaxed);
    f.engineSampleRate   = sampleRate_;
    f.bufferSize         = numSamples;

    // Spectrum: we skip FFT on RT thread (too expensive).
    // EngineCore computes spectrum in a Non-RT thread from the SHM frame.
    // For now, fill bins with zero — UI will render silence-level spectrum.
    std::memset(f.spectrum, 0, sizeof(f.spectrum));

    // Drop if queue full — next block will overwrite
    metricsQueue_.try_enqueue(f);
}

//=============================================================================
// Command application (RT thread — absolutely no heap, no mutex, no I/O)
//=============================================================================

void AudioPipeline::apply(const SetVolumeCmd& cmd) noexcept
{
    // AudioProcessor::setOutputGain takes dB directly
    audioProcessor_.setOutputGain(cmd.gainDb);
}

void AudioPipeline::apply(const SetBypassCmd& cmd) noexcept
{
    masterBypassed_.store(cmd.bypass != 0, std::memory_order_relaxed);
}

void AudioPipeline::apply(const LoadPluginCmd& cmd) noexcept
{
    // Plugin loading is Non-RT (disk I/O, heap allocation).
    // EngineCore intercepts this before it reaches the queue for RT.
    // If it arrives here it means EngineCore forwarded it after loading —
    // we just signal the chain to activate the slot.
    juce::ignoreUnused(cmd);
}

void AudioPipeline::apply(const RemovePluginCmd& cmd) noexcept
{
    // removePlugin is Non-RT safe (chain guards with shared_mutex internally)
    vstChain_.removePlugin(cmd.slot);
}

void AudioPipeline::apply(const MovePluginCmd& cmd) noexcept
{
    vstChain_.movePlugin(cmd.fromSlot, cmd.toSlot);
}

void AudioPipeline::apply(const SetPluginBypassCmd& cmd) noexcept
{
    vstChain_.setPluginBypass(cmd.slot, cmd.bypass != 0);
}

void AudioPipeline::apply(const SetPluginMixCmd& cmd) noexcept
{
    if (auto* plugin = vstChain_.getPlugin(cmd.slot))
        plugin->setMix(cmd.mix);
}

void AudioPipeline::apply(const SetInputGainCmd& cmd) noexcept
{
    audioProcessor_.setInputGain(cmd.gainDb);
}

void AudioPipeline::apply(const SetOutputGainCmd& cmd) noexcept
{
    audioProcessor_.setOutputGain(cmd.gainDb);
}

void AudioPipeline::apply(const ShutdownCmd&) noexcept
{
    // Signal the app to quit — use JUCEApplication::quit() from a Non-RT timer
    juce::MessageManager::callAsync([]
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    });
}

} // namespace ARC
