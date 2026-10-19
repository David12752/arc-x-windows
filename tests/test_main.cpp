/*
 * ============================================================================
 * ARC System Audio Host - Test Main Entry Point
 * ============================================================================
 */

#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

// Initialize JUCE for tests
#include "JuceHeader.h"

// Initialize logger for tests
#include "Utils/Logger.h"

class TestInitializer
{
public:
    TestInitializer()
    {
        // Initialize JUCE
        juce::initialiseJuce_GUI();
        
        // Initialize logger to a test file
        juce::File logFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("ARC_Test_Log.txt");
        ARC::Logger::initialize(logFile);
    }
    
    ~TestInitializer()
    {
        ARC::Logger::shutdown();
        juce::shutdownJuce_GUI();
    }
};

// Global initializer
static TestInitializer g_testInitializer;

