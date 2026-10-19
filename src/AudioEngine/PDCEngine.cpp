/*
 * ============================================================================
 * ARC System Audio Host - Plugin Delay Compensation Engine Implementation
 * ============================================================================
 */

#include "PDCEngine.h"
#include "../Utils/Logger.h"

namespace ARC
{

//=============================================================================
PDCEngine::PDCEngine()
{
    // Initialise both ping-pong descriptors as "not ready"
    descA.ready = false;
    descB.ready = false;
    activeDesc.store(&descA, std::memory_order_release);
    stagingDesc = &descB;

    Logger::log("PDCEngine created");
}

//=============================================================================
void PDCEngine::reconfigure(const std::array<int, MAX_SLOTS>& slotLatencies,
                             int numActiveSlots,
                             double sampleRate,
                             int blockSize)
{
    juce::ignoreUnused(blockSize);

    // Clamp to MAX_SLOTS
    const int numSlots = juce::jlimit(0, MAX_SLOTS, numActiveSlots);

    // -------------------------------------------------------------------------
    // Build new descriptor in the staging buffer (Non-RT, may allocate)
    // -------------------------------------------------------------------------
    stagingDesc->numSlots      = numSlots;
    stagingDesc->limiterLatency = limiterLatency;
    stagingDesc->totalLatency  = limiterLatency;

    for (int i = 0; i < numSlots; ++i)
    {
        const int lat = juce::jlimit(0, MAX_DELAY_SAMPLES, slotLatencies[i]);
        stagingDesc->slots[i].latencySamples = lat;
        stagingDesc->totalLatency += lat;
    }
    for (int i = numSlots; i < MAX_SLOTS; ++i)
        stagingDesc->slots[i].latencySamples = 0;

    // -------------------------------------------------------------------------
    // Resize delay lines that need to grow (never shrink — avoids realloc on
    // minor changes; the difference is at most a few hundred samples of RAM)
    // -------------------------------------------------------------------------
    for (int i = 0; i < numSlots; ++i)
    {
        const int needed = stagingDesc->slots[i].latencySamples;
        auto& dl = delayLines[i];

        // Capacity includes at least needed+1 to allow the ring arithmetic
        const int minCapacity = needed + 1;
        if (static_cast<int>(dl.ring[0].size()) < minCapacity)
        {
            // Round up to next power of two for efficient modulo
            const int capacity = juce::nextPowerOfTwo(minCapacity);
            dl.resize(capacity);
        }

        dl.delayLength = needed;
    }

    // -------------------------------------------------------------------------
    // Atomically publish the new descriptor to the RT thread
    // -------------------------------------------------------------------------
    stagingDesc->ready = true;
    auto* prev = activeDesc.exchange(stagingDesc, std::memory_order_acq_rel);

    // The old active descriptor becomes the new staging buffer
    prev->ready = false;
    stagingDesc = prev;

    Logger::log("PDCEngine reconfigured: " +
                juce::String(numSlots) + " slots, total latency = " +
                juce::String(stagingDesc == &descA ? descB.totalLatency
                                                   : descA.totalLatency) +
                " samples (" +
                juce::String(static_cast<double>(stagingDesc == &descA
                                                     ? descB.totalLatency
                                                     : descA.totalLatency) /
                             sampleRate * 1000.0, 2) +
                " ms @ " + juce::String(sampleRate) + " Hz)");
}

//=============================================================================
void PDCEngine::notifyLimiterLatency(int samples)
{
    limiterLatency = juce::jmax(0, samples);
    // Full reconfigure() required to propagate — caller is responsible.
}

//=============================================================================
bool PDCEngine::compensateDryPath(int slotIndex,
                                   juce::AudioBuffer<float>& dryBuffer,
                                   int numSamples)
{
    // RT-safe: single atomic load, no lock
    const auto* desc = activeDesc.load(std::memory_order_acquire);
    if (desc == nullptr || !desc->ready)
        return false;

    if (slotIndex < 0 || slotIndex >= desc->numSlots)
        return false;

    const int delay = desc->slots[slotIndex].latencySamples;
    if (delay == 0)
        return true; // nothing to do

    auto& dl = delayLines[slotIndex];
    if (dl.delayLength != delay)
        return false; // descriptor not yet consistent with delay lines — skip

    // Apply in-place delay (RT-safe: only accesses pre-allocated ring)
    dl.processInPlace(dryBuffer, numSamples);
    return true;
}

//=============================================================================
int PDCEngine::getTotalLatencySamples() const noexcept
{
    const auto* desc = activeDesc.load(std::memory_order_acquire);
    if (desc == nullptr || !desc->ready)
        return 0;
    return desc->totalLatency;
}

double PDCEngine::getTotalLatencyMs(double sampleRate) const noexcept
{
    if (sampleRate <= 0.0)
        return 0.0;
    return static_cast<double>(getTotalLatencySamples()) / sampleRate * 1000.0;
}

//=============================================================================
void PDCEngine::reset()
{
    for (auto& dl : delayLines)
        dl.clear();

    Logger::log("PDCEngine reset");
}

} // namespace ARC
