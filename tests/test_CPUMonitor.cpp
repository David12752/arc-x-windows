/*
 * ============================================================================
 * ARC System Audio Host - CPU Monitor Unit Tests
 * ============================================================================
 */

#include <catch2/catch_test_macros.hpp>
#include "Utils/CPUMonitor.h"
#include "Utils/Constants.h"

using namespace ARC;

TEST_CASE("CPUMonitor - Initialization", "[CPUMonitor]")
{
    CPUMonitor monitor;
    
    REQUIRE(monitor.getCurrentUsage() == 0.0);
    REQUIRE(monitor.getPeakUsage() == 0.0);
    REQUIRE(monitor.getAverageUsage() == 0.0);
}

TEST_CASE("CPUMonitor - Basic Measurement", "[CPUMonitor]")
{
    CPUMonitor monitor;
    
    // Set callback parameters
    monitor.setCallbackParameters(48000.0, 256);
    
    // Simulate processing
    monitor.startMeasurement();
    // Simulate some work
    juce::Thread::sleep(1); // 1ms
    monitor.endMeasurement();
    
    auto data = monitor.getData();
    REQUIRE(data.currentUsage >= 0.0);
    REQUIRE(data.currentUsage <= 100.0);
}

TEST_CASE("CPUMonitor - Reset", "[CPUMonitor]")
{
    CPUMonitor monitor;
    
    monitor.setCallbackParameters(48000.0, 256);
    monitor.startMeasurement();
    juce::Thread::sleep(1);
    monitor.endMeasurement();
    
    // Should have some usage
    REQUIRE(monitor.getCurrentUsage() > 0.0);
    
    // Reset
    monitor.reset();
    
    REQUIRE(monitor.getCurrentUsage() == 0.0);
    REQUIRE(monitor.getPeakUsage() == 0.0);
    REQUIRE(monitor.getAverageUsage() == 0.0);
}

TEST_CASE("CPUMonitor - High CPU Threshold", "[CPUMonitor]")
{
    CPUMonitor monitor;
    
    monitor.setHighCPUThreshold(50.0);
    monitor.setCallbackParameters(48000.0, 256);
    
    // Initially should not be high
    REQUIRE_FALSE(monitor.isHighCPU());
}

