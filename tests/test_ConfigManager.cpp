/*
 * ============================================================================
 * ARC System Audio Host - Config Manager Unit Tests
 * ============================================================================
 */

#include <catch2/catch_test_macros.hpp>
#include "Utils/ConfigManager.h"
#include <juce_core/juce_core.h>

using namespace ARC;

TEST_CASE("ConfigManager - Basic Operations", "[ConfigManager]")
{
    // Create temporary config file
    juce::File tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("ARC_Test_Config.xml");
    
    ConfigManager config;
    config.initialize(tempFile);
    
    // Test set/get
    config.setValue("testBool", true);
    REQUIRE(config.getValue("testBool", false) == true);
    
    config.setValue("testInt", 42);
    REQUIRE(config.getValue("testInt", 0) == 42);
    
    config.setValue("testDouble", 3.14);
    REQUIRE(config.getValue("testDouble", 0.0) == Approx(3.14));
    
    config.setValue("testString", juce::String("Hello"));
    REQUIRE(config.getValue("testString", juce::String()) == "Hello");
    
    // Cleanup
    tempFile.deleteFile();
}

TEST_CASE("ConfigManager - Default Values", "[ConfigManager]")
{
    juce::File tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("ARC_Test_Config2.xml");
    
    ConfigManager config;
    config.initialize(tempFile);
    
    // Test default values
    REQUIRE(config.getValue("nonexistent", true) == true);
    REQUIRE(config.getValue("nonexistent", 100) == 100);
    REQUIRE(config.getValue("nonexistent", juce::String("default")) == "default");
    
    // Cleanup
    tempFile.deleteFile();
}

