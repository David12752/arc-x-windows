/*
 * ============================================================================
 * ARC System Audio Host - Shared Memory Metrics Publisher
 *
 * Writes MetricsFrame data into a Windows Named Shared Memory region using
 * the seqlock protocol for wait-free, consistent reads from the Electron UI.
 *
 * SEQLOCK WRITE GUARANTEE
 * -----------------------
 * 1. Increment sequence to ODD  (reader will spin)
 * 2. Release fence
 * 3. Write MetricsFrame fields
 * 4. Release fence
 * 5. Increment sequence to EVEN (reader will accept this frame)
 *
 * This means a reader that catches us mid-write will just retry once and
 * never see a torn frame.  Zero contention, zero mutex on write path.
 *
 * PUMPING
 * -------
 * publish() is called from a juce::HighResolutionTimer (60Hz) on the Non-RT
 * engine timer thread.  It reads the latest MetricsFrame from the SPSC queue
 * written by the RT callback and atomically commits it to shared memory.
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include "../Concurrency/MetricsFrame.h"
#include "../Concurrency/SPSCQueue.h"
#include <atomic>
#include <cstring>

namespace ARC
{

//=============================================================================
/**
 * MetricsPublisher
 *
 * Owns the Windows shared memory region and implements the seqlock write side.
 * Typically owned by EngineCore and ticked at 60Hz.
 *
 * Usage:
 *   MetricsPublisher publisher;
 *   publisher.open();            // call once at startup
 *
 *   // Non-RT timer thread (60Hz):
 *   MetricsFrame latest;
 *   if (metricsQueue.try_dequeue(latest))
 *       publisher.publish(latest);
 *
 *   publisher.close();           // call at shutdown
 */
class MetricsPublisher
{
public:
    //=========================================================================
    MetricsPublisher()  = default;
    ~MetricsPublisher() { close(); }

    MetricsPublisher(const MetricsPublisher&)            = delete;
    MetricsPublisher& operator=(const MetricsPublisher&) = delete;

    //=========================================================================
    /**
     * Create (or open) the shared memory region and map it into this process.
     * @return true on success.
     */
    bool open();

    /** Unmap and close the shared memory. Safe to call if not opened. */
    void close();

    bool isOpen() const noexcept { return mappedView_ != nullptr; }

    //=========================================================================
    /**
     * Write a MetricsFrame into shared memory using the seqlock protocol.
     * Must be called from a single writer thread only.
     *
     * @param frame  The metrics snapshot to publish.
     */
    void publish(const MetricsFrame& frame) noexcept;

    //=========================================================================
    /**
     * Write a "zeroed" frame to signal that the engine has stopped.
     * The UI can use engineRunning == 0 to show an idle state.
     */
    void publishIdle() noexcept;

private:
    //=========================================================================
    /** Returns pointer to the SHMHeader within the mapped view. */
    SHMHeader*    header()    noexcept;
    /** Returns pointer to the MetricsFrame within the mapped view. */
    MetricsFrame* dataFrame() noexcept;

    //=========================================================================
    void*     mappedView_  {nullptr};  // result of MapViewOfFile
    void*     hMapping_    {nullptr};  // HANDLE from CreateFileMappingA

    uint64_t  frameCounter_{0};        // monotonically increasing

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MetricsPublisher)
};

} // namespace ARC
