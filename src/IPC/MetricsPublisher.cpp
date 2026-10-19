/*
 * ============================================================================
 * ARC System Audio Host - Shared Memory Metrics Publisher Implementation
 * ============================================================================
 */

#include "MetricsPublisher.h"
#include "../Utils/Logger.h"
#include <atomic>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ARC
{

//=============================================================================
bool MetricsPublisher::open()
{
    if (mappedView_ != nullptr)
        return true; // already open

#ifdef _WIN32
    // Create or open the named shared memory region.
    // If the Electron process already created it we just open it.
    HANDLE hMap = CreateFileMappingA(
        INVALID_HANDLE_VALUE,   // back with system paging file
        nullptr,                // default security
        PAGE_READWRITE,
        0,
        static_cast<DWORD>(ARC_SHM_SIZE),
        ARC_SHM_NAME);

    if (hMap == nullptr || hMap == INVALID_HANDLE_VALUE)
    {
        Logger::error("MetricsPublisher: CreateFileMappingA failed, error=" +
                      juce::String(static_cast<int>(GetLastError())));
        return false;
    }

    void* view = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, ARC_SHM_SIZE);
    if (view == nullptr)
    {
        Logger::error("MetricsPublisher: MapViewOfFile failed, error=" +
                      juce::String(static_cast<int>(GetLastError())));
        CloseHandle(hMap);
        return false;
    }

    hMapping_   = hMap;
    mappedView_ = view;

    // Initialise header
    SHMHeader* hdr = header();
    hdr->magic     = 0x41524332u; // "ARC2"
    hdr->frameSize = static_cast<uint32_t>(sizeof(MetricsFrame));
    hdr->sequence  = 0;

    // Zero out the frame area
    std::memset(dataFrame(), 0, sizeof(MetricsFrame));

    Logger::log("MetricsPublisher: SHM opened (" +
                juce::String(static_cast<int>(ARC_SHM_SIZE)) + " bytes, name=" +
                juce::String(ARC_SHM_NAME) + ")");
    return true;

#else
    Logger::warning("MetricsPublisher: Shared Memory is Windows-only in this build.");
    return false;
#endif
}

//=============================================================================
void MetricsPublisher::close()
{
    if (mappedView_ == nullptr)
        return;

    publishIdle(); // zero frame so the UI knows the engine is gone

#ifdef _WIN32
    UnmapViewOfFile(mappedView_);
    CloseHandle(static_cast<HANDLE>(hMapping_));
#endif

    mappedView_ = nullptr;
    hMapping_   = nullptr;

    Logger::log("MetricsPublisher: SHM closed");
}

//=============================================================================
void MetricsPublisher::publish(const MetricsFrame& frame) noexcept
{
    if (mappedView_ == nullptr)
        return;

    SHMHeader*    hdr  = header();
    MetricsFrame* dest = dataFrame();

    // Tag frame with a monotonic counter
    MetricsFrame tagged = frame;
    tagged.sequenceNumber = ++frameCounter_;

    // --- SEQLOCK WRITE ---
    // Step 1: Set sequence odd (write in progress)
    uint64_t seq = hdr->sequence + 1u;
    // Use reinterpret_cast to write through the raw uint64_t in the header.
    // We use std::atomic_ref (C++20) if available, else a volatile store plus fence.
    std::atomic_ref<uint64_t>(hdr->sequence).store(seq, std::memory_order_release);

    // Step 2: Write the frame data
    std::memcpy(dest, &tagged, sizeof(MetricsFrame));

    // Step 3: Set sequence even (stable — reader can accept)
    std::atomic_ref<uint64_t>(hdr->sequence).store(seq + 1u, std::memory_order_release);
}

//=============================================================================
void MetricsPublisher::publishIdle() noexcept
{
    MetricsFrame idle{};
    idle.engineRunning = 0;
    publish(idle);
}

//=============================================================================
SHMHeader* MetricsPublisher::header() noexcept
{
    return static_cast<SHMHeader*>(mappedView_);
}

MetricsFrame* MetricsPublisher::dataFrame() noexcept
{
    return reinterpret_cast<MetricsFrame*>(
        static_cast<uint8_t*>(mappedView_) + sizeof(SHMHeader));
}

} // namespace ARC
