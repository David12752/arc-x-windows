/*
 * ============================================================================
 * ARC System Audio Host - CPU Monitor Implementation
 * ============================================================================
 */

#include "CPUMonitor.h"
#include "Logger.h"

namespace ARC
{

//=============================================================================
CPUMonitor::CPUMonitor()
{
    usageHistory.fill(0.0);
}

CPUMonitor::~CPUMonitor() = default;

//=============================================================================
void CPUMonitor::startMeasurement()
{
    measurementStart = juce::Time::getHighResolutionTicks();
}

void CPUMonitor::endMeasurement()
{
    auto endTime = juce::Time::getHighResolutionTicks();
    double duration = juce::Time::highResolutionTicksToSeconds(endTime - measurementStart);

    // Calculate percentage of callback time used
    double usage = (duration / callbackDuration) * 100.0;

    updateStatistics(usage);
}

//=============================================================================
void CPUMonitor::setCallbackDuration(double durationSeconds)
{
    callbackDuration = durationSeconds;
}

void CPUMonitor::setCallbackParameters(double sampleRate, int bufferSize)
{
    callbackDuration = bufferSize / sampleRate;
}

//=============================================================================
CPUMonitorData CPUMonitor::getData() const
{
    CPUMonitorData data;
    data.currentUsage = currentUsage.load();
    data.peakUsage = peakUsage.load();
    data.averageUsage = averageUsage.load();

    std::lock_guard<std::mutex> lock(historyMutex);
    data.sampleCount = historyCount;

    if (historyCount > 0)
    {
        data.minUsage = *std::min_element(usageHistory.begin(),
                                          usageHistory.begin() + historyCount);
        data.maxUsage = *std::max_element(usageHistory.begin(),
                                          usageHistory.begin() + historyCount);
    }

    return data;
}

double CPUMonitor::getCurrentUsage() const
{
    return currentUsage.load();
}

double CPUMonitor::getPeakUsage() const
{
    return peakUsage.load();
}

double CPUMonitor::getAverageUsage() const
{
    return averageUsage.load();
}

//=============================================================================
void CPUMonitor::reset()
{
    currentUsage.store(0.0);
    peakUsage.store(0.0);
    averageUsage.store(0.0);

    std::lock_guard<std::mutex> lock(historyMutex);
    usageHistory.fill(0.0);
    historyIndex = 0;
    historyCount = 0;
    historySum = 0.0;
}

void CPUMonitor::resetPeak()
{
    peakUsage.store(currentUsage.load());
}

//=============================================================================
void CPUMonitor::setHighCPUThreshold(double percentage)
{
    highCPUThreshold = percentage;
}

bool CPUMonitor::isHighCPU() const
{
    return highCPUFlag.load();
}

void CPUMonitor::setHighCPUCallback(std::function<void()> callback)
{
    highCPUCallback = callback;
}

//=============================================================================
void CPUMonitor::updateStatistics(double usage)
{
    currentUsage.store(usage);

    // Update peak
    double currentPeak = peakUsage.load();
    while (usage > currentPeak)
    {
        if (peakUsage.compare_exchange_weak(currentPeak, usage))
            break;
    }

    // Update history and average
    {
        std::lock_guard<std::mutex> lock(historyMutex);

        // Subtract old value from sum
        historySum -= usageHistory[historyIndex];

        // Add new value
        usageHistory[historyIndex] = usage;
        historySum += usage;

        // Update index
        historyIndex = (historyIndex + 1) % HISTORY_SIZE;
        if (historyCount < HISTORY_SIZE)
        {
            ++historyCount;
        }

        // Calculate average
        if (historyCount > 0)
        {
            averageUsage.store(historySum / historyCount);
        }
    }

    // Check threshold
    bool wasHigh = highCPUFlag.load();
    bool isHigh = usage > highCPUThreshold;
    highCPUFlag.store(isHigh);

    // Trigger callback on transition to high CPU
    if (isHigh && !wasHigh && highCPUCallback)
    {
        juce::MessageManager::callAsync(highCPUCallback);
    }
}

} // namespace ARC

