/*
 * ============================================================================
 * ARC System Audio Host - Shared Memory Metrics Frame
 *
 * This POD struct is the single unit of data exchanged between the C++ engine
 * and the Electron UI via a Windows Named Shared Memory region.
 *
 * SHARED MEMORY LAYOUT (arc_metrics_shm, 64 KB)
 * -----------------------------------------------
 *
 *   Offset 0:  SHMHeader   (16 bytes)
 *   Offset 16: MetricsFrame (≤ 512 bytes typical)
 *
 * SEQLOCK WRITE PROTOCOL (writer = C++ engine, reader = Electron N-API addon)
 * ---------------------------------------------------------------------------
 *   Write:
 *     1. Increment header.sequence to an ODD value (signals "write in progress")
 *     2. std::atomic_thread_fence(release)
 *     3. Write MetricsFrame fields
 *     4. std::atomic_thread_fence(release)
 *     5. Increment header.sequence to an EVEN value (signals "write complete")
 *
 *   Read (Electron, 60Hz):
 *     Loop:
 *       seq1 = header.sequence.load(acquire)
 *       if (seq1 & 1) continue;   // write in progress — spin
 *       copy MetricsFrame
 *       seq2 = header.sequence.load(acquire)
 *       if (seq1 == seq2) break;  // consistent read
 *
 * This guarantees the reader always sees a complete, consistent frame
 * with zero contention on the write path (no mutex, no CAS).
 *
 * IMPORTANT: All fields must remain POD (no vtable, no pointers, no STL).
 *            Layout must be identical in C++, TypeScript (DataView), and
 *            any future Python/C# consumer.
 * ============================================================================
 */

#pragma once

#include <cstdint>
#include <atomic>

namespace ARC
{

//=============================================================================
// Number of spectrum bins exposed to UI.
// Matches the coarse-binned display resolution (not the full FFT).
constexpr int METRICS_SPECTRUM_BINS = 64;

//=============================================================================
#pragma pack(push, 1)

/**
 * SHMHeader — occupies the first 16 bytes of the shared memory region.
 * Only the engine process writes here. Electron reads sequence for seqlock.
 */
struct SHMHeader
{
    // Seqlock counter. Even = stable, Odd = write in progress.
    uint64_t sequence{0};

    // ARC magic to detect version mismatch between engine and UI builds.
    uint32_t magic{0x41524332};    // "ARC2"
    uint32_t frameSize{0};         // sizeof(MetricsFrame) — filled by writer
};

/**
 * MetricsFrame — the payload written at offset sizeof(SHMHeader).
 * All floats are linear (0.0–1.0 for levels) unless noted.
 */
struct MetricsFrame
{
    // --- Identity ---
    uint64_t sequenceNumber{0};    // monotonically increasing frame counter
    uint64_t timestampMs{0};       // engine wall clock (GetTickCount64)

    // --- Levels (linear, 0.0 = silence, 1.0 = 0 dBFS) ---
    float peakL{0.0f};
    float peakR{0.0f};
    float rmsL{0.0f};
    float rmsR{0.0f};
    float peakHoldL{0.0f};
    float peakHoldR{0.0f};

    // --- Master limiter ---
    float limiterGainReductionDb{0.0f};  // 0 = no reduction, negative = attenuating

    // --- CPU ---
    float cpuPercent{0.0f};              // 0–100
    float cpuPeakPercent{0.0f};

    // --- Latency ---
    int32_t totalLatencySamples{0};
    float   totalLatencyMs{0.0f};
    int32_t xrunCount{0};

    // --- Engine state flags ---
    uint8_t engineRunning{0};    // 1 = audio callback active
    uint8_t masterBypassed{0};   // 1 = entire chain bypassed
    uint8_t srcActive{0};        // 1 = sample rate conversion in use
    uint8_t reserved0{0};

    // --- Chain ---
    int32_t activePluginCount{0};
    // Per-slot CPU (up to 10 slots)
    float   slotCpuPercent[10]{};
    uint8_t slotBypassed[10]{};
    uint8_t pad0[2]{};           // alignment

    // --- Sample rate info ---
    double  engineSampleRate{48000.0};
    double  loopbackSampleRate{48000.0};
    int32_t bufferSize{256};
    uint8_t pad1[4]{};

    // --- Spectrum (64 bins, log-scaled magnitudes, 0.0–1.0) ---
    // Bin 0 = ~20 Hz, Bin 63 = ~20 kHz
    float spectrum[METRICS_SPECTRUM_BINS]{};
};

#pragma pack(pop)

// Compile-time size checks — catch accidental padding/alignment changes
static_assert(sizeof(SHMHeader)   == 16,   "SHMHeader layout changed");
static_assert(sizeof(MetricsFrame) <= 512, "MetricsFrame exceeds budget");

// Total shared memory size
constexpr std::size_t ARC_SHM_SIZE = 65536; // 64 KB — plenty of headroom
constexpr const char* ARC_SHM_NAME = "arc_metrics_shm";

} // namespace ARC
