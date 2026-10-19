/*
 * ============================================================================
 * ARC System Audio Host - Plugin Host Header
 * Core VST3 plugin hosting infrastructure
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include <memory>
#include <vector>
#include <functional>
#include <atomic>

namespace ARC
{

//=============================================================================
/**
 * Plugin format types
 */
enum class PluginFormat
{
    VST3,
    Unknown
};

//=============================================================================
/**
 * Plugin load result
 */
struct PluginLoadResult
{
    bool success{false};
    juce::String errorMessage;
    std::unique_ptr<juce::AudioPluginInstance> plugin;
};

//=============================================================================
/**
 * Plugin host class
 * Manages plugin loading, instantiation, and lifecycle
 */
class PluginHost
{
public:
    //=========================================================================
    PluginHost();
    ~PluginHost();

    //=========================================================================
    // Initialization
    void initialize();
    void shutdown();

    //=========================================================================
    // Plugin format management
    juce::AudioPluginFormatManager& getFormatManager();
    juce::KnownPluginList& getKnownPluginList();

    //=========================================================================
    // Plugin loading
    PluginLoadResult loadPlugin(const juce::PluginDescription& description,
                                double sampleRate, int blockSize);

    PluginLoadResult loadPluginFromFile(const juce::File& file,
                                        double sampleRate, int blockSize);

    //=========================================================================
    // Plugin instantiation helpers
    std::unique_ptr<juce::AudioPluginInstance> createPluginInstance(
        const juce::PluginDescription& description,
        double sampleRate, int blockSize,
        juce::String& errorMessage);

    //=========================================================================
    // Plugin scanning
    void scanForPlugins(const juce::File& path,
                       std::function<void(float, const juce::String&)> progressCallback = nullptr);

    void scanForAllPlugins(std::function<void(float, const juce::String&)> progressCallback = nullptr);

    //=========================================================================
    // Plugin list management
    void addPlugin(const juce::PluginDescription& description);
    void removePlugin(const juce::PluginDescription& description);
    void clearPluginList();

    //=========================================================================
    // Blacklist management (for crashed plugins)
    void blacklistPlugin(const juce::String& pluginId);
    void removeFromBlacklist(const juce::String& pluginId);
    bool isBlacklisted(const juce::String& pluginId) const;
    juce::StringArray getBlacklistedPlugins() const;
    void clearBlacklist();

    //=========================================================================
    // Plugin search
    juce::PluginDescription findPluginByName(const juce::String& name) const;
    juce::PluginDescription findPluginById(const juce::String& id) const;
    std::vector<juce::PluginDescription> findPluginsByManufacturer(const juce::String& manufacturer) const;
    std::vector<juce::PluginDescription> findPluginsByCategory(const juce::String& category) const;

    //=========================================================================
    // Plugin paths
    void addPluginSearchPath(const juce::File& path);
    void removePluginSearchPath(const juce::File& path);
    juce::Array<juce::File> getPluginSearchPaths() const;

    //=========================================================================
    // Persistence
    void savePluginList(const juce::File& file);
    void loadPluginList(const juce::File& file);
    void saveBlacklist(const juce::File& file);
    void loadBlacklist(const juce::File& file);

    //=========================================================================
    // IK Multimedia ARC 3 specific
    juce::PluginDescription findARCPlugin() const;
    bool isARCPluginAvailable() const;

    //=========================================================================
    // Callbacks
    using PluginListChangeCallback = std::function<void()>;
    void addPluginListChangeCallback(PluginListChangeCallback callback);

private:
    //=========================================================================
    void notifyPluginListChanged();

    //=========================================================================
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPluginList;
    juce::Array<juce::File> searchPaths;
    juce::StringArray blacklist;

    std::vector<PluginListChangeCallback> listChangeCallbacks;
    mutable std::mutex callbackMutex;

    bool initialized{false};

    // Plugin scanner thread
    std::unique_ptr<juce::PluginListComponent> scannerComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginHost)
};

//=============================================================================
/**
 * Plugin instance wrapper with additional state
 */
class PluginInstanceWrapper
{
public:
    PluginInstanceWrapper(std::unique_ptr<juce::AudioPluginInstance> instance,
                          const juce::PluginDescription& desc);
    ~PluginInstanceWrapper();

    //=========================================================================
    juce::AudioPluginInstance* getPlugin() const { return plugin.get(); }
    const juce::PluginDescription& getDescription() const { return description; }

    //=========================================================================
    // State management
    void saveState(juce::MemoryBlock& destData);
    void loadState(const void* data, int sizeInBytes);

    //=========================================================================
    // Bypass
    void setBypass(bool bypass);
    bool isBypassed() const { return bypassed.load(); }

    //=========================================================================
    // CPU monitoring
    void startCPUMeasurement();
    void endCPUMeasurement();
    double getCPUUsage() const { return cpuUsage.load(); }
    double getAverageCPUUsage() const;

    //=========================================================================
    // Processing
    void prepareToPlay(double sampleRate, int blockSize);
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer);
    void releaseResources();

    //=========================================================================
    // Editor
    bool hasEditor() const;
    juce::AudioProcessorEditor* createEditor();

    //=========================================================================
    // Parameters
    int getNumParameters() const;
    float getParameter(int index) const;
    void setParameter(int index, float value);
    juce::String getParameterName(int index) const;
    juce::String getParameterText(int index) const;

private:
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    juce::PluginDescription description;

    std::atomic<bool> bypassed{false};

    // CPU monitoring
    std::atomic<double> cpuUsage{0.0};
    double cpuMeasurementStart{0.0};
    juce::Array<double> cpuHistory;
    static constexpr int CPU_HISTORY_SIZE = 100;

    // Audio bypass buffer
    juce::AudioBuffer<float> bypassBuffer;

    double currentSampleRate{44100.0};
    int currentBlockSize{512};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginInstanceWrapper)
};

} // namespace ARC

