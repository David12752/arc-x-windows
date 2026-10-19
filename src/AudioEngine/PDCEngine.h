/*
 * ============================================================================
 * ARC System Audio Host - Plugin Delay Compensation Engine
 *
 * WHY PDC MATTERS IN A SERIAL CHAIN
 * ----------------------------------
 * When a plugin in the chain is bypassed (or has a wet/dry mix < 1.0), the
 * dry signal takes a zero-latency path while every plugin *after* it still
 * introduces its own latency.  Without compensation this creates a phase
 * discontinuity every time bypass is toggled — audible as a transient click
 * or, worse, as subtle comb-filtering on partially-wet slots.
 *
 * The PDC engine inserts a per-slot delay line whose length equals the
 * latency of that slot's plugin.  This delay is applied to the bypass path
 * so that toggling bypass leaves total chain latency constant.
 *
 * ALGORITHM
 * ---------
 * Given a chain of N plugins with per-plugin latencies L[0..N-1]:
 *
 *   totalLatency = sum(L[i])
 *
 * For slot k the bypass-path delay line length is L[k].  The full (wet) path
 * still runs through the plugin as normal.  After mixing, everything at the
 * chain output is aligned to the same time reference.
 *
 * NOTE: The LookAheadLimiter also contributes latency (LOOKAHEAD_SAMPLES).
 * Call notifyLimiterLatency() after limiter prepare() so the total is correct.
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
 * PDCEngine — Plugin Delay Compensation for the VST chain.
 *
 * Thread safety:
 *   reconfigure()  — called from Non-RT thread when chain changes.
 *   process()      — called from RT thread per audio block.
 *
 * These two are protected by a simple atomic "generation" mechanism:
 * the Non-RT thread rebuilds into a staging buffer, then atomically
 * swaps the active descriptor.  The RT thread reads the active descriptor
 * once per block (atomic load).  No mutex is taken in the RT path.
 */
class PDCEngine
{
public:
    //=========================================================================
    static constexpr int MAX_SLOTS        = 10;
    static constexpr int MAX_DELAY_SAMPLES = 192000; // 4 s @ 48 kHz — should be far more than enough

    //=========================================================================
    PDCEngine();
    ~PDCEngine() = default;

    //=========================================================================
    // Non-RT: call whenever the chain topology or plugin latencies change.
    // slotLatencies[i] = latency of plugin in slot i (0 if slot is empty).
    void reconfigure(const std::array<int, MAX_SLOTS>& slotLatencies,
                     int numActiveSlots,
                     double sampleRate,
                     int blockSize);

    // Non-RT: inform engine of limiter look-ahead so total is accurate.
    void notifyLimiterLatency(int samples);

    //=========================================================================
    // RT: compensate a single slot's bypass path.
    //
    //   Call this BEFORE the plugin processes the buffer for slot `slotIndex`.
    //   If the slot plugin is bypassed (or has a mix < 1.0), pass a pointer to
    //   the pre-plugin dry buffer here.  The engine delays it by L[slotIndex]
    //   so it aligns with the downstream wet signal.
    //
    //   Returns true when delay lines are ready and compensation was applied.
    bool compensateDryPath(int slotIndex,
                           juce::AudioBuffer<float>& dryBuffer,
                           int numSamples);

    //=========================================================================
    // RT: report total latency of the currently-active descriptor.
    int getTotalLatencySamples() const noexcept;
    double getTotalLatencyMs(double sampleRate) const noexcept;

    //=========================================================================
    // Non-RT: reset all delay lines (e.g. after a seek / chain clear).
    void reset();

private:
    //=========================================================================
    struct SlotDesc
    {
        int latencySamples{0};
    };

    struct ActiveDescriptor
    {
        std::array<SlotDesc, MAX_SLOTS> slots{};
        int  numSlots{0};
        int  totalLatency{0};
        int  limiterLatency{0};
        bool ready{false};
    };

    //=========================================================================
    // Per-slot circular delay lines — sized to MAX_DELAY_SAMPLES at prepare().
    // Per-channel: we support up to 2 channels (stereo).
    struct DelayLine
    {
        static constexpr int MAX_CH = 2;
        std::array<std::vector<float>, MAX_CH> ring;
        int writeHead{0};
        int delayLength{0}; // active delay in samples

        void resize(int capacity)
        {
            for (auto& ch : ring)
            {
                ch.assign(static_cast<size_t>(capacity), 0.0f);
            }
            writeHead  = 0;
            delayLength = 0;
        }

        void clear()
        {
            for (auto& ch : ring)
                std::fill(ch.begin(), ch.end(), 0.0f);
            writeHead = 0;
        }

        // Write `numSamples` from `src` and read back with `delayLength` delay.
        // Input and output may point to the same buffer (in-place).
        void processInPlace(juce::AudioBuffer<float>& buffer, int numSamples)
        {
            if (delayLength == 0)
                return; // zero-latency slot — no delay needed

            const int capacity    = static_cast<int>(ring[0].size());
            const int numCh       = std::min(buffer.getNumChannels(),
                                             static_cast<int>(MAX_CH));

            for (int s = 0; s < numSamples; ++s)
            {
                const int writePos = writeHead % capacity;
                const int readPos  = (writeHead - delayLength + capacity * 16) % capacity;

                for (int ch = 0; ch < numCh; ++ch)
                {
                    // Write incoming sample
                    ring[ch][writePos] = buffer.getSample(ch, s);
                    // Overwrite with delayed sample
                    buffer.setSample(ch, s, ring[ch][readPos]);
                }

                ++writeHead;
            }
        }
    };

    //=========================================================================
    std::array<DelayLine, MAX_SLOTS> delayLines;

    // Atomic active descriptor — allows lock-free RT read
    std::atomic<ActiveDescriptor*> activeDesc{nullptr};

    // Two ping-pong descriptors (avoid heap allocation in RT)
    ActiveDescriptor descA;
    ActiveDescriptor descB;
    ActiveDescriptor* stagingDesc{&descB}; // Non-RT writes to this
    std::atomic<bool> pendingSwap{false};

    int limiterLatency{0}; // non-RT only
};

} // namespace ARC
