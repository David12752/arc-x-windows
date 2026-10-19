/*
 * ============================================================================
 * ARC System Audio Host - Real-Time Audio Pipeline
 *
 * AudioPipeline is the single object that sits on the RT audio callback.
 * It owns:
 *   - Command dispatch from the SPSC queue (Non-RT commands → RT application)
 *   - Audio routing: WASAPILoopback → VSTChain → AudioProcessor (limiter)
 *   - Metrics capture: level, latency, CPU → SPSC queue for the 60Hz publisher
 *
 * RT SAFETY RULES (strictly enforced in this file)
 * -------------------------------------------------
 *  NO  std::mutex / std::lock_guard
 *  NO  heap allocation (new / delete / std::vector resize / std::string concat)
 *  NO  file I/O, socket I/O, pipe I/O
 *  NO  blocking OS calls (Sleep, WaitForSingleObject)
 *  YES std::atomic load/store (relaxed or acquire/release)
 *  YES SPSCQueue::try_enqueue / try_dequeue
 *  YES memcpy / memset
 *  YES calls to VSTChain::processBlock (which itself is RT-safe post Phase 0)
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include "../AudioEngine/AudioDeviceManager.h"
#include "../AudioEngine/AudioProcessor.h"
#include "../VST/VSTChain.h"
#include "../IPC/EngineCommand.h"
#include "../Concurrency/SPSCQueue.h"
#include "../Concurrency/MetricsFrame.h"
#include "../Concurrency/AudioBlock.h"
#include <atomic>
#include <array>
#include <cstdint>

namespace ARC
{

//=============================================================================
/**
 * AudioPipeline — implements AudioCallback (called on the RT audio thread).
 *
 * Owned by EngineCore.  References to VSTChain and AudioProcessor are
 * injected at construction and must outlive the pipeline.
 */
class AudioPipeline : public AudioCallback
{
public:
    //=========================================================================
    using CommandQueue = SPSCQueue<EngineCommand, 256>;
    using MetricsQueue = SPSCQueue<MetricsFrame,  64>;

    /**
     * @param vstChain       RT-safe plugin chain (Phase 0 post-fix).
     * @param audioProcessor Post-chain DSP (limiter, output gain).
     * @param commandQueue   Commands arrive here from the CommandServer thread.
     * @param metricsQueue   Metrics snapshots are pushed here for the 60Hz timer.
     */
    AudioPipeline(VSTChain&        vstChain,
                  AudioProcessor&  audioProcessor,
                  CommandQueue&    commandQueue,
                  MetricsQueue&    metricsQueue);

    ~AudioPipeline() override = default;

    //=========================================================================
    // AudioCallback interface (called on RT thread)
    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midiBuffer) override;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    //=========================================================================
    // State accessors — safe to call from any thread (atomic reads)
    bool  isRunning()       const noexcept { return running_.load(std::memory_order_relaxed); }
    float getLastPeakL()    const noexcept { return peakL_.load(std::memory_order_relaxed); }
    float getLastPeakR()    const noexcept { return peakR_.load(std::memory_order_relaxed); }
    float getCpuPercent()   const noexcept { return cpuPercent_.load(std::memory_order_relaxed); }
    int   getXrunCount()    const noexcept { return xrunCount_.load(std::memory_order_relaxed); }
    void  resetXrunCount()         noexcept { xrunCount_.store(0, std::memory_order_relaxed); }

    //=========================================================================
    // Non-RT command helpers (bypasses the queue for immediate settings)
    // Safe to call BEFORE start — do NOT call during active audio processing.
    void setMasterBypass(bool bypass) noexcept
    {
        masterBypassed_.store(bypass, std::memory_order_relaxed);
    }

    //=========================================================================
    // Loopback queue — set before audio starts.
    // If set, processBlock pulls a captured AudioBlock from here as its input
    // instead of using whatever the device driver provided (which may be silence).
    using LoopbackQueue = SPSCQueue<AudioBlock, 16>;
    void setLoopbackQueue(LoopbackQueue* q) noexcept { loopbackQueue_ = q; }

private:
    //=========================================================================
    // RT-thread only — no locks, no heap, no I/O
    void drainCommandQueue()  noexcept;
    void captureMetrics(const juce::AudioBuffer<float>& buffer,
                        double blockStartTimeMs)        noexcept;

    // Command application — all called on RT thread
    void apply(const SetVolumeCmd&)       noexcept;
    void apply(const SetBypassCmd&)       noexcept;
    void apply(const LoadPluginCmd&)      noexcept;
    void apply(const RemovePluginCmd&)    noexcept;
    void apply(const MovePluginCmd&)      noexcept;
    void apply(const SetPluginBypassCmd&) noexcept;
    void apply(const SetPluginMixCmd&)    noexcept;
    void apply(const SetInputGainCmd&)    noexcept;
    void apply(const SetOutputGainCmd&)   noexcept;
    void apply(const ShutdownCmd&)        noexcept;

    // Default no-op for commands that need Non-RT handling
    template<typename T>
    void apply(const T&) noexcept { /* Non-RT command — handled upstream */ }

    //=========================================================================
    VSTChain&       vstChain_;
    AudioProcessor& audioProcessor_;
    CommandQueue&   commandQueue_;
    MetricsQueue&   metricsQueue_;

    // Lock-free loopback queue: WASAPILoopback → processBlock.
    // Nullptr when not in loopback-queue mode (e.g. unit tests, GUI-only).
    LoopbackQueue*  loopbackQueue_{nullptr};

    // RT state (atomics only)
    std::atomic<bool>  running_        {false};
    std::atomic<bool>  masterBypassed_ {false};
    std::atomic<float> peakL_          {0.0f};
    std::atomic<float> peakR_          {0.0f};
    std::atomic<float> cpuPercent_     {0.0f};
    std::atomic<int>   xrunCount_      {0};

    // Computed in prepareToPlay, read on RT thread — no atomic needed
    // because prepareToPlay is called before the callback starts.
    double sampleRate_    {48000.0};
    int    samplesPerBlock_{256};

    // Peak hold decay (per-block, set in prepareToPlay)
    float  peakHoldDecay_  {0.995f};

    // Hold accumulators (RT-thread local — no sharing)
    float  peakHoldL_{0.0f};
    float  peakHoldR_{0.0f};

    // RMS accumulators
    float  rmsAccL_{0.0f};
    float  rmsAccR_{0.0f};

    // Frame counter for metrics (RT thread local)
    uint64_t frameCounter_{0};

    // Tick counter for metering decimation (publish every ~N blocks ≈ 60Hz)
    int  metricsTick_{0};
    int  metricsPublishInterval_{1};  // set in prepareToPlay

    juce::MidiBuffer dummyMidi_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPipeline)
};

} // namespace ARC
