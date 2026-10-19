/*
 * ============================================================================
 * ARC System Audio Host - VST Scanner Implementation
 * ============================================================================
 */

#include "VSTScanner.h"
#include "../Utils/Logger.h"

namespace ARC
{

//=============================================================================
VSTScanner::VSTScanner()
    : Thread("VST Scanner Thread")
{
    formatManager.addDefaultFormats();
    setDefaultSearchPaths();
    Logger::log("VSTScanner created");
}

VSTScanner::~VSTScanner()
{
    stopScan();
    Logger::log("VSTScanner destroyed");
}

//=============================================================================
void VSTScanner::addSearchPath(const juce::File& path)
{
    if (!searchPaths.contains(path))
    {
        searchPaths.add(path);
        Logger::log("Added VST search path: " + path.getFullPathName());
    }
}

void VSTScanner::removeSearchPath(const juce::File& path)
{
    searchPaths.removeFirstMatchingValue(path);
    Logger::log("Removed VST search path: " + path.getFullPathName());
}

void VSTScanner::clearSearchPaths()
{
    searchPaths.clear();
    Logger::log("Cleared all VST search paths");
}

juce::Array<juce::File> VSTScanner::getSearchPaths() const
{
    return searchPaths;
}

void VSTScanner::setDefaultSearchPaths()
{
    searchPaths.clear();

    // Standard VST3 locations on Windows
    searchPaths.add(juce::File("C:\\Program Files\\Common Files\\VST3"));
    searchPaths.add(juce::File("C:\\Program Files (x86)\\Common Files\\VST3"));

    // User VST3 folder
    auto userVst3 = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("VST3");
    if (userVst3.exists())
    {
        searchPaths.add(userVst3);
    }

    Logger::log("Set default VST search paths (" + juce::String(searchPaths.size()) + " paths)");
}

//=============================================================================
void VSTScanner::startBackgroundScan()
{
    if (isScanning())
    {
        Logger::log("Scan already in progress");
        return;
    }

    Logger::log("Starting background VST scan...");
    Logger::log("Search paths: " + juce::String(searchPaths.size()));
    for (const auto& path : searchPaths)
    {
        Logger::log("  - " + path.getFullPathName() + (path.exists() ? " (exists)" : " (not found)"));
    }
    startThread(juce::Thread::Priority::low);
}

void VSTScanner::startScan()
{
    if (isScanning())
    {
        Logger::log("Scan already in progress");
        return;
    }

    scanning.store(true);

    {
        std::lock_guard<std::mutex> lock(resultsMutex);
        scanResults.clear();
    }

    // Clear known plugin list to force a fresh scan
    // This ensures we scan all plugins, not just new ones
    knownPluginList.clear();

    currentProgress = ScanProgress();
    currentProgress.totalCount = 0;

    Logger::log("Starting plugin scan in " + juce::String(searchPaths.size()) + " search paths");
    Logger::log("Format manager has " + juce::String(formatManager.getNumFormats()) + " formats");

    // Log all formats
    for (int i = 0; i < formatManager.getNumFormats(); ++i)
    {
        auto* format = formatManager.getFormats()[i];
        if (format)
        {
            Logger::log("  Format " + juce::String(i) + ": " + format->getName());
        }
    }

    // Scan each path
    int pathIndex = 0;
    for (const auto& path : searchPaths)
    {
        if (!scanning.load())
            break;

        pathIndex++;
        Logger::log("Scanning path " + juce::String(pathIndex) + "/" + juce::String(searchPaths.size()) + ": " + path.getFullPathName());

        if (path.exists())
        {
            scanPath(path);
        }
        else
        {
            Logger::warning("Path does not exist: " + path.getFullPathName());
        }
    }

    scanning.store(false);
    notifyCompletion(true);

    int totalPlugins = knownPluginList.getNumTypes();
    Logger::log("VST scan complete. Found " + juce::String(totalPlugins) + " plugins total");
    Logger::log("Scan results: " + juce::String(currentProgress.validCount) + " valid, " + 
                juce::String(currentProgress.failedCount) + " failed");
}

void VSTScanner::stopScan()
{
    if (!isScanning())
        return;

    Logger::log("Stopping VST scan...");
    scanning.store(false);
    stopThread(5000);
}

bool VSTScanner::isScanning() const
{
    return scanning.load() || isThreadRunning();
}

//=============================================================================
void VSTScanner::run()
{
    Logger::log("VSTScanner thread started");
    startScan();
    Logger::log("VSTScanner thread finished");
}

//=============================================================================
std::vector<PluginScanResult> VSTScanner::getScanResults() const
{
    std::lock_guard<std::mutex> lock(resultsMutex);
    return scanResults;
}

std::vector<juce::PluginDescription> VSTScanner::getValidPlugins() const
{
    std::vector<juce::PluginDescription> plugins;

    // Use knownPluginList which is updated by PluginDirectoryScanner
    // This is more reliable than scanResults
    for (const auto& desc : knownPluginList.getTypes())
    {
        plugins.push_back(desc);
    }

    return plugins;
}

int VSTScanner::getPluginCount() const
{
    return knownPluginList.getNumTypes();
}

//=============================================================================
juce::PluginDescription VSTScanner::findPlugin(const juce::String& name) const
{
    for (const auto& desc : knownPluginList.getTypes())
    {
        if (desc.name.containsIgnoreCase(name))
        {
            return desc;
        }
    }
    return {};
}

juce::PluginDescription VSTScanner::findPluginById(const juce::String& id) const
{
    for (const auto& desc : knownPluginList.getTypes())
    {
        if (desc.createIdentifierString() == id)
        {
            return desc;
        }
    }
    return {};
}

std::vector<juce::PluginDescription> VSTScanner::findPluginsByManufacturer(
    const juce::String& manufacturer) const
{
    std::vector<juce::PluginDescription> result;

    for (const auto& desc : knownPluginList.getTypes())
    {
        if (desc.manufacturerName.containsIgnoreCase(manufacturer))
        {
            result.push_back(desc);
        }
    }

    return result;
}

std::vector<juce::PluginDescription> VSTScanner::findEffects() const
{
    std::vector<juce::PluginDescription> result;

    for (const auto& desc : knownPluginList.getTypes())
    {
        if (!desc.isInstrument)
        {
            result.push_back(desc);
        }
    }

    return result;
}

std::vector<juce::PluginDescription> VSTScanner::findInstruments() const
{
    std::vector<juce::PluginDescription> result;

    for (const auto& desc : knownPluginList.getTypes())
    {
        if (desc.isInstrument)
        {
            result.push_back(desc);
        }
    }

    return result;
}

//=============================================================================
juce::PluginDescription VSTScanner::findARCPlugin() const
{
    // Look for IK Multimedia ARC
    for (const auto& desc : knownPluginList.getTypes())
    {
        if (desc.manufacturerName.containsIgnoreCase("IK Multimedia"))
        {
            if (desc.name.containsIgnoreCase("ARC"))
            {
                Logger::log("Found ARC plugin: " + desc.name);
                return desc;
            }
        }
    }

    // Also check for "ARC System" or "ARC 3"
    for (const auto& desc : knownPluginList.getTypes())
    {
        if (desc.name.containsIgnoreCase("ARC System") ||
            desc.name.containsIgnoreCase("ARC 3") ||
            desc.name.containsIgnoreCase("ARC 4"))
        {
            Logger::log("Found ARC plugin: " + desc.name);
            return desc;
        }
    }

    return {};
}

bool VSTScanner::hasARCPlugin() const
{
    auto desc = findARCPlugin();
    return desc.name.isNotEmpty();
}

//=============================================================================
void VSTScanner::addToFavorites(const juce::String& pluginId)
{
    if (!favorites.contains(pluginId))
    {
        favorites.add(pluginId);
        Logger::log("Added plugin to favorites: " + pluginId);
    }
}

void VSTScanner::removeFromFavorites(const juce::String& pluginId)
{
    favorites.removeString(pluginId);
    Logger::log("Removed plugin from favorites: " + pluginId);
}

bool VSTScanner::isFavorite(const juce::String& pluginId) const
{
    return favorites.contains(pluginId);
}

std::vector<juce::PluginDescription> VSTScanner::getFavoritePlugins() const
{
    std::vector<juce::PluginDescription> result;

    for (const auto& id : favorites)
    {
        auto desc = findPluginById(id);
        if (desc.name.isNotEmpty())
        {
            result.push_back(desc);
        }
    }

    return result;
}

//=============================================================================
void VSTScanner::saveCache(const juce::File& file)
{
    Logger::log("Saving plugin cache to: " + file.getFullPathName());

    auto xml = knownPluginList.createXml();
    if (xml != nullptr)
    {
        xml->writeTo(file);
    }

    // Save favorites
    auto favFile = file.withFileExtension("favorites");
    favFile.replaceWithText(favorites.joinIntoString("\n"));
}

void VSTScanner::loadCache(const juce::File& file)
{
    Logger::log("Loading plugin cache from: " + file.getFullPathName());

    if (file.existsAsFile())
    {
        auto xml = juce::XmlDocument::parse(file);
        if (xml != nullptr)
        {
            knownPluginList.recreateFromXml(*xml);
        }
    }

    // Load favorites
    auto favFile = file.withFileExtension("favorites");
    if (favFile.existsAsFile())
    {
        favorites.clear();
        favorites.addLines(favFile.loadFileAsString());
    }
}

void VSTScanner::clearCache()
{
    knownPluginList.clear();

    std::lock_guard<std::mutex> lock(resultsMutex);
    scanResults.clear();

    Logger::log("Plugin cache cleared");
}

bool VSTScanner::isCacheValid(const juce::File& file) const
{
    if (!file.existsAsFile())
        return false;

    // Check if cache is older than 7 days
    auto modTime = file.getLastModificationTime();
    auto now = juce::Time::getCurrentTime();
    auto age = now - modTime;

    return age.inDays() < 7;
}

//=============================================================================
void VSTScanner::setProgressCallback(ProgressCallback callback)
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    progressCallback = callback;
}

void VSTScanner::setCompletionCallback(CompletionCallback callback)
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    completionCallback = callback;
}

//=============================================================================
juce::KnownPluginList& VSTScanner::getKnownPluginList()
{
    return knownPluginList;
}

juce::AudioPluginFormatManager& VSTScanner::getFormatManager()
{
    return formatManager;
}

//=============================================================================
void VSTScanner::scanPath(const juce::File& path)
{
    Logger::log("Scanning path: " + path.getFullPathName());

    if (!path.exists())
    {
        Logger::warning("Scan path does not exist: " + path.getFullPathName());
        return;
    }

    for (auto* format : formatManager.getFormats())
    {
        if (!scanning.load())
            break;

        // Use JUCE's PluginDirectoryScanner which correctly handles VST3 folders
        juce::FileSearchPath searchPath(path.getFullPathName());
        juce::PluginDirectoryScanner scanner(
            knownPluginList, *format, searchPath, true, juce::File(), false);

        juce::String pluginName;
        // Use false to force rescan even if plugin is already in list
        // This ensures we get all plugins on first scan
        while (scanner.scanNextFile(false, pluginName))
        {
            if (!scanning.load())
                break;

            Logger::log("    Scanning plugin: " + pluginName);

            // Update progress
            updateProgress(pluginName);

            // Check if plugin was successfully added to knownPluginList
            // The scanner adds it during scanNextFile, so check immediately after
            int countBefore = knownPluginList.getNumTypes();
            
            // Wait a tiny bit for the scanner to finish adding
            juce::Thread::sleep(10);
            
            int countAfter = knownPluginList.getNumTypes();
            
            PluginScanResult result;
            result.scanTime = juce::Time::getCurrentTime();
            
            if (countAfter > countBefore)
            {
                // Plugin was successfully added
                result.valid = true;
                currentProgress.validCount++;
                
                // Find the description for this plugin from knownPluginList
                for (const auto& desc : knownPluginList.getTypes())
                {
                    if (desc.name == pluginName)
                    {
                        result.description = desc;
                        result.file = juce::File(desc.fileOrIdentifier);
                        Logger::log("  Found: " + result.description.name);
                        break;
                    }
                }
            }
            else
            {
                // Plugin scan failed or was skipped
                result.valid = false;
                result.errorMessage = "Plugin scan failed or skipped";
                currentProgress.failedCount++;
                Logger::log("  Failed to scan: " + pluginName);
            }

            {
                std::lock_guard<std::mutex> lock(resultsMutex);
                scanResults.push_back(result);
            }

            currentProgress.scannedCount++;
        }
    }
}

void VSTScanner::scanFile(const juce::File& file)
{
    updateProgress(file.getFileName());

    PluginScanResult result;
    result.file = file;
    result.scanTime = juce::Time::getCurrentTime();

    try
    {
        juce::OwnedArray<juce::PluginDescription> descriptions;

        for (auto* format : formatManager.getFormats())
        {
            if (format->fileMightContainThisPluginType(file.getFullPathName()))
            {
                format->findAllTypesForFile(descriptions, file.getFullPathName());
            }
        }

        if (!descriptions.isEmpty())
        {
            result.description = *descriptions[0];
            result.valid = true;

            // Add to known plugin list
            for (auto* desc : descriptions)
            {
                knownPluginList.addType(*desc);
            }

            currentProgress.validCount++;
            Logger::log("  Found: " + result.description.name);
        }
        else
        {
            result.valid = false;
            result.errorMessage = "No valid plugin found in file";
            currentProgress.failedCount++;
        }
    }
    catch (const std::exception& e)
    {
        result.valid = false;
        result.errorMessage = juce::String("Exception: ") + e.what();
        currentProgress.failedCount++;
        Logger::log("  Error scanning " + file.getFileName() + ": " + e.what());
    }

    {
        std::lock_guard<std::mutex> lock(resultsMutex);
        scanResults.push_back(result);
    }

    currentProgress.scannedCount++;
}

void VSTScanner::updateProgress(const juce::String& currentFile)
{
    currentProgress.currentFile = currentFile;
    currentProgress.overallProgress = currentProgress.totalCount > 0
        ? static_cast<float>(currentProgress.scannedCount) / currentProgress.totalCount
        : 0.0f;

    std::lock_guard<std::mutex> lock(callbackMutex);
    if (progressCallback)
    {
        // Post to message thread
        juce::MessageManager::callAsync([this, progress = currentProgress]()
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            if (progressCallback)
            {
                progressCallback(progress);
            }
        });
    }
}

void VSTScanner::notifyCompletion(bool success)
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    if (completionCallback)
    {
        juce::MessageManager::callAsync([this, success]()
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            if (completionCallback)
            {
                completionCallback(success);
            }
        });
    }
}

} // namespace ARC

