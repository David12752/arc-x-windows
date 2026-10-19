/*
 * ============================================================================
 * ARC System Audio Host - VST Scanner Header
 * Scans and catalogs VST3 plugins on the system
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include <vector>
#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>

namespace ARC
{

//=============================================================================
/**
 * Scan result for a single plugin
 */
struct PluginScanResult
{
    juce::PluginDescription description;
    juce::File file;
    bool valid{false};
    juce::String errorMessage;
    juce::Time scanTime;
};

//=============================================================================
/**
 * Scan progress information
 */
struct ScanProgress
{
    float overallProgress{0.0f};
    juce::String currentFile;
    int scannedCount{0};
    int totalCount{0};
    int validCount{0};
    int failedCount{0};
};

//=============================================================================
/**
 * VST Scanner class
 * Handles scanning, caching, and management of VST3 plugins
 */
class VSTScanner : public juce::Thread
{
public:
    //=========================================================================
    using ProgressCallback = std::function<void(const ScanProgress&)>;
    using CompletionCallback = std::function<void(bool)>;

    //=========================================================================
    VSTScanner();
    ~VSTScanner() override;

    //=========================================================================
    // Search paths
    void addSearchPath(const juce::File& path);
    void removeSearchPath(const juce::File& path);
    void clearSearchPaths();
    juce::Array<juce::File> getSearchPaths() const;
    void setDefaultSearchPaths();

    //=========================================================================
    // Scanning
    void startBackgroundScan();
    void startScan();
    void stopScan();
    bool isScanning() const;

    //=========================================================================
    // Results
    std::vector<PluginScanResult> getScanResults() const;
    std::vector<juce::PluginDescription> getValidPlugins() const;
    int getPluginCount() const;

    //=========================================================================
    // Specific plugin search
    juce::PluginDescription findPlugin(const juce::String& name) const;
    juce::PluginDescription findPluginById(const juce::String& id) const;
    std::vector<juce::PluginDescription> findPluginsByManufacturer(
        const juce::String& manufacturer) const;
    std::vector<juce::PluginDescription> findEffects() const;
    std::vector<juce::PluginDescription> findInstruments() const;

    //=========================================================================
    // IK Multimedia ARC specific
    juce::PluginDescription findARCPlugin() const;
    bool hasARCPlugin() const;

    //=========================================================================
    // Favorites
    void addToFavorites(const juce::String& pluginId);
    void removeFromFavorites(const juce::String& pluginId);
    bool isFavorite(const juce::String& pluginId) const;
    std::vector<juce::PluginDescription> getFavoritePlugins() const;

    //=========================================================================
    // Cache management
    void saveCache(const juce::File& file);
    void loadCache(const juce::File& file);
    void clearCache();
    bool isCacheValid(const juce::File& file) const;

    //=========================================================================
    // Callbacks
    void setProgressCallback(ProgressCallback callback);
    void setCompletionCallback(CompletionCallback callback);

    //=========================================================================
    // Known plugin list access (for JUCE integration)
    juce::KnownPluginList& getKnownPluginList();
    juce::AudioPluginFormatManager& getFormatManager();

protected:
    //=========================================================================
    // Thread interface
    void run() override;

private:
    //=========================================================================
    void scanPath(const juce::File& path);
    void scanFile(const juce::File& file);
    void updateProgress(const juce::String& currentFile);
    void notifyCompletion(bool success);

    //=========================================================================
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPluginList;

    juce::Array<juce::File> searchPaths;
    std::vector<PluginScanResult> scanResults;
    juce::StringArray favorites;

    std::atomic<bool> scanning{false};
    ScanProgress currentProgress;

    ProgressCallback progressCallback;
    CompletionCallback completionCallback;
    mutable std::mutex callbackMutex;
    mutable std::mutex resultsMutex;

    juce::File cacheFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VSTScanner)
};

} // namespace ARC

