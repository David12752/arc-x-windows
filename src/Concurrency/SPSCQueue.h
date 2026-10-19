/*
 * ============================================================================
 * ARC System Audio Host - Lock-Free SPSC Queue
 *
 * Single-Producer Single-Consumer wait-free ring queue.
 *
 * Properties:
 *   - Wait-free enqueue (producer) and dequeue (consumer).
 *   - No heap allocation after construction.
 *   - Cache-line isolated head and tail counters to eliminate
 *     false sharing (critical for RT audio callback performance).
 *   - Capacity must be a compile-time power of two.
 *
 * Thread safety:
 *   Exactly ONE thread calls try_enqueue() (producer).
 *   Exactly ONE thread calls try_dequeue() (consumer).
 *   These may be different threads — that is the intended usage.
 *   Calling enqueue and dequeue from the SAME thread is also safe.
 *
 * Usage example (RT audio ↔ Non-RT metrics):
 *
 *   SPSCQueue<MetricsFrame, 64> q;
 *
 *   // RT thread (producer):
 *   MetricsFrame f = buildFrame();
 *   q.try_enqueue(f);   // drops if full — acceptable for metering
 *
 *   // Non-RT thread (consumer, 60Hz timer):
 *   MetricsFrame f;
 *   while (q.try_dequeue(f))
 *       publisher.write(f);
 *
 * If you link cameron314/readerwriterqueue, replace this header with a
 * thin wrapper around `moodycamel::ReaderWriterQueue<T>` for production.
 * ============================================================================
 */

#pragma once

#include <atomic>
#include <array>
#include <type_traits>
#include <cstddef>

namespace ARC
{

//=============================================================================
/**
 * SPSCQueue<T, Capacity>
 *
 * @tparam T        Element type. Must be trivially copyable for RT safety.
 * @tparam Capacity Ring size. MUST be a power of two. Checked at compile time.
 */
template<typename T, std::size_t Capacity>
class SPSCQueue
{
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "SPSCQueue: Capacity must be a power of two.");
    static_assert(std::is_trivially_copyable_v<T>,
                  "SPSCQueue: T must be trivially copyable (no heap, no locks).");

public:
    //=========================================================================
    SPSCQueue() noexcept = default;
    ~SPSCQueue()         = default;

    // Non-copyable, non-movable (atomic members)
    SPSCQueue(const SPSCQueue&)            = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;

    //=========================================================================
    /**
     * Try to enqueue one item. Called by the PRODUCER thread only.
     *
     * @return true  if the item was enqueued.
     *         false if the queue was full (item dropped — caller decides policy).
     */
    [[nodiscard]] bool try_enqueue(const T& item) noexcept
    {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        const std::size_t next = (tail + 1) & MASK;

        // If next == head, the ring is full
        if (next == head_.load(std::memory_order_acquire))
            return false;

        buffer_[tail] = item;
        tail_.store(next, std::memory_order_release);
        return true;
    }

    //=========================================================================
    /**
     * Try to dequeue one item. Called by the CONSUMER thread only.
     *
     * @param[out] item  Populated with the dequeued value on success.
     * @return true  if an item was dequeued.
     *         false if the queue was empty.
     */
    [[nodiscard]] bool try_dequeue(T& item) noexcept
    {
        const std::size_t head = head_.load(std::memory_order_relaxed);

        if (head == tail_.load(std::memory_order_acquire))
            return false; // empty

        item = buffer_[head];
        head_.store((head + 1) & MASK, std::memory_order_release);
        return true;
    }

    //=========================================================================
    /** Approximate number of items currently in the queue. Not exact under concurrency. */
    std::size_t size_approx() const noexcept
    {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        const std::size_t head = head_.load(std::memory_order_relaxed);
        return (tail - head + Capacity) & MASK;
    }

    bool empty() const noexcept
    {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    static constexpr std::size_t capacity() noexcept { return Capacity; }

private:
    //=========================================================================
    static constexpr std::size_t MASK = Capacity - 1;

    // Cache-line separation prevents false sharing between producer and consumer.
    // On x86-64 a cache line is 64 bytes.
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};

    // The ring buffer itself. Placed after the atomics so the first two cache
    // lines are dedicated to head and tail — no eviction from unrelated writes.
    std::array<T, Capacity> buffer_{};
};

} // namespace ARC
