/*
 * ============================================================================
 * ARC System Audio Host - CPU Monitor Header
 * Real-time CPU usage monitoring for audio processing
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include "Constants.h"
#include <atomic>
#include <array>
#include <mutex>

namespace ARC
{

//=============================================================================
/**
 * @struct CPUMonitorData
 * @brief CPU monitoring data structure
 * 
 * Contains real-time CPU usage statistics for audio processing.
 * All values are percentages (0.0 - 100.0).
 */
struct CPUMonitorData
{
    double currentUsage{0.0};      ///< Current CPU usage percentage
    double peakUsage{0.0};        ///< Peak CPU usage since last reset
    double averageUsage{0.0};     ///< Average CPU usage over history
    double minUsage{0.0};          ///< Minimum CPU usage in history
    double maxUsage{0.0};          ///< Maximum CPU usage in history
    int sampleCount{0};            ///< Number of samples in history
};

//=============================================================================
/**
 * @class CPUMonitor
 * @brief Real-time CPU usage monitoring for audio processing
 * 
 * This class tracks CPU usage during audio callbacks and provides
 * statistics including current, peak, and average usage.
 * 
 * Thread-safe: Can be called from audio thread (startMeasurement/endMeasurement)
 * and UI thread (getData/getCurrentUsage).
 * 
 * @example
 * @code
 * CPUMonitor monitor;
 * monitor.setCallbackParameters(48000.0, 256);
 * 
 * // In audio callback:
 * monitor.startMeasurement();
 * // ... process audio ...
 * monitor.endMeasurement();
 * 
 * // In UI thread:
 * double usage = monitor.getCurrentUsage();
 * @endcode
 */
class CPUMonitor
{
public:
    //=========================================================================
    /** @brief Constructor */
    CPUMonitor();
    
    /** @brief Destructor */
    ~CPUMonitor();

    //=========================================================================
    // Timing methods (call from audio thread)
    void startMeasurement();
    void endMeasurement();

    //=========================================================================
    // Set the expected callback duration for percentage calculation
    void setCallbackDuration(double durationSeconds);
    void setCallbackParameters(double sampleRate, int bufferSize);

    //=========================================================================
    // Get monitoring data (thread-safe)
    CPUMonitorData getData() const;
    double getCurrentUsage() const;
    double getPeakUsage() const;
    double getAverageUsage() const;

    //=========================================================================
    // Reset statistics
    void reset();
    void resetPeak();

    //=========================================================================
    // Threshold alerts
    void setHighCPUThreshold(double percentage);
    bool isHighCPU() const;
    void setHighCPUCallback(std::function<void()> callback);

private:
    //=========================================================================
    void updateStatistics(double usage);

    //=========================================================================
    std::atomic<double> currentUsage{0.0};
    std::atomic<double> peakUsage{0.0};
    std::atomic<double> averageUsage{0.0};

    double callbackDuration{Constants::DEFAULT_CALLBACK_DURATION_MS / 1000.0}; // Default 5ms
    double measurementStart{0.0};

    // Running average calculation
    static constexpr int HISTORY_SIZE = Constants::CPU_HISTORY_SIZE;
    std::array<double, HISTORY_SIZE> usageHistory{};
    int historyIndex{0};
    int historyCount{0};
    double historySum{0.0};
    mutable std::mutex historyMutex;

    // Alert threshold
    double highCPUThreshold{Constants::HIGH_CPU_THRESHOLD_PERCENT};
    std::atomic<bool> highCPUFlag{false};
    std::function<void()> highCPUCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CPUMonitor)
};

//=============================================================================
/**
 * Scoped CPU measurement helper
 * RAII wrapper for automatic measurement
 */
class ScopedCPUMeasurement
{
public:
    explicit ScopedCPUMeasurement(CPUMonitor& monitor)
        : cpuMonitor(monitor)
    {
        cpuMonitor.startMeasurement();
    }

    ~ScopedCPUMeasurement()
    {
        cpuMonitor.endMeasurement();
    }

private:
    CPUMonitor& cpuMonitor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScopedCPUMeasurement)
};

} // namespace ARC

