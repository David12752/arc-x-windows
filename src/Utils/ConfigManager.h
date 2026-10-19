/*
 * ============================================================================
 * ARC System Audio Host - Config Manager Header
 * Application configuration and settings management
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include <map>
#include <mutex>

namespace ARC
{

//=============================================================================
/**
 * Configuration manager class
 * Handles application settings storage and retrieval
 */
class ConfigManager
{
public:
    //=========================================================================
    ConfigManager();
    ~ConfigManager();

    //=========================================================================
    // Initialization
    void initialize(const juce::File& configFile);

    //=========================================================================
    // Save/Load
    void save();
    void load();
    void reset();

    //=========================================================================
    // Value access - generic template methods
    template<typename T>
    T getValue(const juce::String& key, const T& defaultValue) const;

    template<typename T>
    void setValue(const juce::String& key, const T& value);

    //=========================================================================
    // Specific type methods for common types
    bool getValue(const juce::String& key, bool defaultValue) const;
    int getValue(const juce::String& key, int defaultValue) const;
    double getValue(const juce::String& key, double defaultValue) const;
    juce::String getValue(const juce::String& key, const juce::String& defaultValue) const;
    juce::StringArray getValue(const juce::String& key, const juce::StringArray& defaultValue) const;

    void setValue(const juce::String& key, bool value);
    void setValue(const juce::String& key, int value);
    void setValue(const juce::String& key, double value);
    void setValue(const juce::String& key, const juce::String& value);
    void setValue(const juce::String& key, const juce::StringArray& value);

    //=========================================================================
    // Key management
    bool hasKey(const juce::String& key) const;
    void removeKey(const juce::String& key);
    juce::StringArray getAllKeys() const;

    //=========================================================================
    // File path
    juce::File getConfigFile() const { return configFile; }
    juce::File getConfigDirectory() const;

    //=========================================================================
    // Export/Import
    bool exportConfig(const juce::File& file) const;
    bool importConfig(const juce::File& file);

private:
    //=========================================================================
    juce::File configFile;
    std::unique_ptr<juce::PropertiesFile> properties;
    std::unique_ptr<juce::PropertiesFile::Options> options;

    mutable std::mutex mutex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConfigManager)
};

//=============================================================================
/**
 * Default configuration values
 */
namespace DefaultConfig
{
    // Audio
    constexpr double defaultSampleRate = 48000.0;
    constexpr int defaultBufferSize = 256;

    // UI
    constexpr bool minimizeToTray = true;
    constexpr bool loadLastPreset = true;
    constexpr bool autoStart = false;
    constexpr int theme = 1; // Dark

    // Plugins
    constexpr int maxPlugins = 10;
    constexpr bool scanOnStartup = true;
}

} // namespace ARC

