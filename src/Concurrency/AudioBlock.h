/*
 * ============================================================================
 * ARC System Audio Host - AudioBlock
 *
 * A fixed-size, trivially-copyable POD struct that carries one block of
 * stereo audio between the WASAPILoopback capture thread and the RT output
 * callback via SPSCQueue<AudioBlock, N>.
 *
 * WHY FIXED SIZE?
 * ---------------
 * `SPSCQueue<T, N>` requires T to be trivially copyable — no heap, no
 * destructor, no virtual table. By embedding the sample storage inline
 * (MAX_SAMPLES * MAX_CHANNELS floats) we avoid any dynamic allocation on
 * the hot path.
 *
 * BUFFER DEPTH GUIDANCE
 * ---------------------
 * At 48 kHz / 256-sample blocks, the capture thread delivers ~187 blocks/sec.
 * 8 slots = ~43ms of audio headroom — enough to absorb OS scheduling jitter
 * without audible underrun.  Increase to 16 if xruns are observed under load.
 *
 * UNDERRUN POLICY
 * ---------------
 * If the output callback finds the queue empty it writes silence and increments
 * the xrun counter (visible in MetricsFrame.xrunCount). The capture thread
 * drops the oldest block if the queue is full.
 * ============================================================================
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace ARC
{

//=============================================================================
struct AudioBlock
{
    // Maximum supported samples per block (covers 1024 @ 96kHz or 512 @ 48kHz).
    // If blockSize > MAX_SAMPLES the block is silenced and the xrun counter
    // is incremented by the capture thread.
    static constexpr int MAX_SAMPLES  = 1024;
    static constexpr int MAX_CHANNELS = 2;

    // Inline sample storage — channel-major layout [ch][sample]
    float  data[MAX_CHANNELS][MAX_SAMPLES]{};

    // Actual content dimensions (may be ≤ MAX_* values)
    int    numChannels{0};
    int    numSamples{0};

    // Source sample rate (after SRC). Used to detect rate changes mid-stream.
    double sampleRate{48000.0};

    // Capture timestamp (GetTickCount64) — used for latency diagnostics.
    uint64_t captureTimeMs{0};

    // True if this block was padded with silence due to a capture underrun.
    uint8_t  isSilence{0};
    uint8_t  pad[7]{};  // explicit padding to 8-byte alignment

    //=========================================================================
    /** Populate from raw per-channel pointers. Silences if numSamples > MAX_SAMPLES. */
    void fill(const float* const* channels, int nChannels, int nSamples, double sr) noexcept
    {
        numChannels = nChannels  < MAX_CHANNELS ? nChannels  : MAX_CHANNELS;
        numSamples  = nSamples   < MAX_SAMPLES  ? nSamples   : MAX_SAMPLES;
        sampleRate  = sr;
        isSilence   = 0;

        if (nSamples > MAX_SAMPLES)
        {
            // Block is larger than our budget — output silence
            for (int ch = 0; ch < numChannels; ++ch)
                std::memset(data[ch], 0, static_cast<std::size_t>(numSamples) * sizeof(float));
            isSilence = 1;
            return;
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (channels != nullptr && channels[ch] != nullptr)
                std::memcpy(data[ch], channels[ch],
                            static_cast<std::size_t>(numSamples) * sizeof(float));
            else
                std::memset(data[ch], 0,
                            static_cast<std::size_t>(numSamples) * sizeof(float));
        }
    }

    /** Fill remaining channels by replicating channel 0 (mono→stereo). */
    void monoToStereo() noexcept
    {
        if (numChannels == 1)
        {
            std::memcpy(data[1], data[0],
                        static_cast<std::size_t>(numSamples) * sizeof(float));
            numChannels = 2;
        }
    }

    /** Write silence into an AudioBlock (no heap, safe on RT thread). */
    void silence() noexcept
    {
        for (int ch = 0; ch < MAX_CHANNELS; ++ch)
            std::memset(data[ch], 0, MAX_SAMPLES * sizeof(float));
        isSilence = 1;
    }
};

// Compile-time safety: SPSCQueue<AudioBlock> requires trivially copyable.
static_assert(std::is_trivially_copyable_v<AudioBlock>,
              "AudioBlock must be trivially copyable for use in SPSCQueue.");

// Size check — one AudioBlock = 2 * 1024 * 4 + padding ≈ 8.2 KB.
// Queue of 8 = ~66 KB.  Queue of 16 = ~132 KB.  Both are acceptable.
static_assert(sizeof(AudioBlock) <= 10240,
              "AudioBlock is larger than expected — check MAX_SAMPLES/MAX_CHANNELS.");

} // namespace ARC
