/*
 * ============================================================================
 * ARC System Audio Host - Plugin Host Implementation
 * ============================================================================
 */

#include "PluginHost.h"
#include "../Utils/Logger.h"

namespace ARC
{

//=============================================================================
// PluginHost Implementation
//=============================================================================

PluginHost::PluginHost()
{
    Logger::log("PluginHost created");
}

PluginHost::~PluginHost()
{
    shutdown();
    Logger::log("PluginHost destroyed");
}

//=============================================================================
void PluginHost::initialize()
{
    if (initialized)
        return;

    Logger::log("Initializing plugin host...");

    // Add format support
    formatManager.addDefaultFormats();

    // Add default search paths
    searchPaths.add(juce::File("C:\\Program Files\\Common Files\\VST3"));
    searchPaths.add(juce::File("C:\\Program Files (x86)\\Common Files\\VST3"));

    // Add user's VST3 folder if it exists
    auto userVst3 = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("VST3");
    if (userVst3.exists())
    {
        searchPaths.add(userVst3);
    }

    initialized = true;
    Logger::log("Plugin host initialized with " + juce::String(searchPaths.size()) + " search paths");
}

void PluginHost::shutdown()
{
    if (!initialized)
        return;

    Logger::log("Shutting down plugin host...");

    knownPluginList.clear();
    searchPaths.clear();

    initialized = false;
}

//=============================================================================
juce::AudioPluginFormatManager& PluginHost::getFormatManager()
{
    return formatManager;
}

juce::KnownPluginList& PluginHost::getKnownPluginList()
{
    return knownPluginList;
}

//=============================================================================
PluginLoadResult PluginHost::loadPlugin(const juce::PluginDescription& description,
                                         double sampleRate, int blockSize)
{
    PluginLoadResult result;

    if (isBlacklisted(description.createIdentifierString()))
    {
        result.errorMessage = "Plugin is blacklisted due to previous crash";
        return result;
    }

    Logger::log("Loading plugin: " + description.name);

    juce::String errorMessage;
    auto plugin = formatManager.createPluginInstance(
        description, sampleRate, blockSize, errorMessage);

    if (plugin != nullptr)
    {
        result.success = true;
        result.plugin = std::move(plugin);
        Logger::log("Plugin loaded successfully: " + description.name);
    }
    else
    {
        result.errorMessage = errorMessage;
        Logger::log("Failed to load plugin: " + description.name + " - " + errorMessage);
    }

    return result;
}

PluginLoadResult PluginHost::loadPluginFromFile(const juce::File& file,
                                                 double sampleRate, int blockSize)
{
    PluginLoadResult result;

    Logger::log("Loading plugin from file: " + file.getFullPathName());

    juce::OwnedArray<juce::PluginDescription> descriptions;

    for (auto* format : formatManager.getFormats())
    {
        if (format->fileMightContainThisPluginType(file.getFullPathName()))
        {
            format->findAllTypesForFile(descriptions, file.getFullPathName());
            break;
        }
    }

    if (descriptions.isEmpty())
    {
        result.errorMessage = "No valid plugin found in file";
        return result;
    }

    return loadPlugin(*descriptions[0], sampleRate, blockSize);
}

//=============================================================================
std::unique_ptr<juce::AudioPluginInstance> PluginHost::createPluginInstance(
    const juce::PluginDescription& description,
    double sampleRate, int blockSize,
    juce::String& errorMessage)
{
    return formatManager.createPluginInstance(description, sampleRate, blockSize, errorMessage);
}

//=============================================================================
void PluginHost::scanForPlugins(const juce::File& path,
                                std::function<void(float, const juce::String&)> progressCallback)
{
    Logger::log("Scanning for plugins in: " + path.getFullPathName());

    for (auto* format : formatManager.getFormats())
    {
        juce::FileSearchPath searchPath(path.getFullPathName());
        juce::PluginDirectoryScanner scanner(
            knownPluginList, *format, searchPath, true, juce::File(), false);

        juce::String pluginName;
        while (scanner.scanNextFile(true, pluginName))
        {
            if (progressCallback)
            {
                progressCallback(scanner.getProgress(), pluginName);
            }
        }
    }

    notifyPluginListChanged();
    Logger::log("Plugin scan complete. Found " + juce::String(knownPluginList.getNumTypes()) + " plugins");
}

void PluginHost::scanForAllPlugins(std::function<void(float, const juce::String&)> progressCallback)
{
    int totalPaths = searchPaths.size();
    int currentPath = 0;

    for (const auto& path : searchPaths)
    {
        if (path.exists())
        {
            scanForPlugins(path, [&](float progress, const juce::String& name)
            {
                if (progressCallback)
                {
                    float overallProgress = (currentPath + progress) / totalPaths;
                    progressCallback(overallProgress, name);
                }
            });
        }
        ++currentPath;
    }
}

//=============================================================================
void PluginHost::addPlugin(const juce::PluginDescription& description)
{
    knownPluginList.addType(description);
    notifyPluginListChanged();
}

void PluginHost::removePlugin(const juce::PluginDescription& description)
{
    for (int i = 0; i < knownPluginList.getNumTypes(); ++i)
    {
        const auto& types = knownPluginList.getTypes();
        if (types[i].createIdentifierString() ==
            description.createIdentifierString())
        {
            knownPluginList.removeType(types[i]);
            break;
        }
    }
    notifyPluginListChanged();
}

void PluginHost::clearPluginList()
{
    knownPluginList.clear();
    notifyPluginListChanged();
}

//=============================================================================
void PluginHost::blacklistPlugin(const juce::String& pluginId)
{
    if (!blacklist.contains(pluginId))
    {
        blacklist.add(pluginId);
        Logger::log("Plugin blacklisted: " + pluginId);
    }
}

void PluginHost::removeFromBlacklist(const juce::String& pluginId)
{
    blacklist.removeString(pluginId);
    Logger::log("Plugin removed from blacklist: " + pluginId);
}

bool PluginHost::isBlacklisted(const juce::String& pluginId) const
{
    return blacklist.contains(pluginId);
}

juce::StringArray PluginHost::getBlacklistedPlugins() const
{
    return blacklist;
}

void PluginHost::clearBlacklist()
{
    blacklist.clear();
    Logger::log("Plugin blacklist cleared");
}

//=============================================================================
juce::PluginDescription PluginHost::findPluginByName(const juce::String& name) const
{
    for (const auto& desc : knownPluginList.getTypes())
    {
        if (desc.name.containsIgnoreCase(name))
        {
            return desc;
        }
    }
    return juce::PluginDescription();
}

juce::PluginDescription PluginHost::findPluginById(const juce::String& id) const
{
    for (const auto& desc : knownPluginList.getTypes())
    {
        if (desc.createIdentifierString() == id)
        {
            return desc;
        }
    }
    return juce::PluginDescription();
}

std::vector<juce::PluginDescription> PluginHost::findPluginsByManufacturer(
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

std::vector<juce::PluginDescription> PluginHost::findPluginsByCategory(
    const juce::String& category) const
{
    std::vector<juce::PluginDescription> result;

    for (const auto& desc : knownPluginList.getTypes())
    {
        if (desc.category.containsIgnoreCase(category))
        {
            result.push_back(desc);
        }
    }

    return result;
}

//=============================================================================
void PluginHost::addPluginSearchPath(const juce::File& path)
{
    if (!searchPaths.contains(path))
    {
        searchPaths.add(path);
        Logger::log("Added plugin search path: " + path.getFullPathName());
    }
}

void PluginHost::removePluginSearchPath(const juce::File& path)
{
    searchPaths.removeFirstMatchingValue(path);
    Logger::log("Removed plugin search path: " + path.getFullPathName());
}

juce::Array<juce::File> PluginHost::getPluginSearchPaths() const
{
    return searchPaths;
}

//=============================================================================
void PluginHost::savePluginList(const juce::File& file)
{
    auto xml = knownPluginList.createXml();
    if (xml != nullptr)
    {
        xml->writeTo(file);
        Logger::log("Plugin list saved to: " + file.getFullPathName());
    }
}

void PluginHost::loadPluginList(const juce::File& file)
{
    if (file.existsAsFile())
    {
        auto xml = juce::XmlDocument::parse(file);
        if (xml != nullptr)
        {
            knownPluginList.recreateFromXml(*xml);
            notifyPluginListChanged();
            Logger::log("Plugin list loaded from: " + file.getFullPathName());
        }
    }
}

void PluginHost::saveBlacklist(const juce::File& file)
{
    juce::StringArray lines;
    for (const auto& id : blacklist)
    {
        lines.add(id);
    }
    file.replaceWithText(lines.joinIntoString("\n"));
    Logger::log("Blacklist saved to: " + file.getFullPathName());
}

void PluginHost::loadBlacklist(const juce::File& file)
{
    if (file.existsAsFile())
    {
        blacklist.clear();
        blacklist.addLines(file.loadFileAsString());
        Logger::log("Blacklist loaded from: " + file.getFullPathName());
    }
}

//=============================================================================
juce::PluginDescription PluginHost::findARCPlugin() const
{
    // Search for IK Multimedia ARC plugins
    for (const auto& desc : knownPluginList.getTypes())
    {
        if (desc.manufacturerName.containsIgnoreCase("IK Multimedia") &&
            desc.name.containsIgnoreCase("ARC"))
        {
            return desc;
        }
    }
    return juce::PluginDescription();
}

bool PluginHost::isARCPluginAvailable() const
{
    auto desc = findARCPlugin();
    return desc.name.isNotEmpty();
}

//=============================================================================
void PluginHost::addPluginListChangeCallback(PluginListChangeCallback callback)
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    listChangeCallbacks.push_back(callback);
}

void PluginHost::notifyPluginListChanged()
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    for (auto& callback : listChangeCallbacks)
    {
        callback();
    }
}

//=============================================================================
// PluginInstanceWrapper Implementation
//=============================================================================

PluginInstanceWrapper::PluginInstanceWrapper(
    std::unique_ptr<juce::AudioPluginInstance> instance,
    const juce::PluginDescription& desc)
    : plugin(std::move(instance)), description(desc)
{
    Logger::log("PluginInstanceWrapper created for: " + desc.name);
}

PluginInstanceWrapper::~PluginInstanceWrapper()
{
    releaseResources();
    Logger::log("PluginInstanceWrapper destroyed for: " + description.name);
}

//=============================================================================
void PluginInstanceWrapper::saveState(juce::MemoryBlock& destData)
{
    if (plugin != nullptr)
    {
        plugin->getStateInformation(destData);
    }
}

void PluginInstanceWrapper::loadState(const void* data, int sizeInBytes)
{
    if (plugin != nullptr)
    {
        plugin->setStateInformation(data, sizeInBytes);
    }
}

//=============================================================================
void PluginInstanceWrapper::setBypass(bool bypass)
{
    bypassed.store(bypass);
}

//=============================================================================
void PluginInstanceWrapper::startCPUMeasurement()
{
    cpuMeasurementStart = juce::Time::getHighResolutionTicks();
}

void PluginInstanceWrapper::endCPUMeasurement()
{
    auto endTime = juce::Time::getHighResolutionTicks();
    double duration = juce::Time::highResolutionTicksToSeconds(endTime - cpuMeasurementStart);
    double bufferDuration = currentBlockSize / currentSampleRate;
    double usage = (duration / bufferDuration) * 100.0;

    cpuUsage.store(usage);

    // Update history
    cpuHistory.add(usage);
    if (cpuHistory.size() > CPU_HISTORY_SIZE)
    {
        cpuHistory.remove(0);
    }
}

double PluginInstanceWrapper::getAverageCPUUsage() const
{
    if (cpuHistory.isEmpty())
        return 0.0;

    double sum = 0.0;
    for (auto value : cpuHistory)
    {
        sum += value;
    }
    return sum / cpuHistory.size();
}

//=============================================================================
void PluginInstanceWrapper::prepareToPlay(double sampleRate, int blockSize)
{
    if (plugin != nullptr)
    {
        currentSampleRate = sampleRate;
        currentBlockSize = blockSize;

        plugin->prepareToPlay(sampleRate, blockSize);
        plugin->setRateAndBufferSizeDetails(sampleRate, blockSize);

        // Prepare bypass buffer
        bypassBuffer.setSize(2, blockSize);
    }
}

void PluginInstanceWrapper::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midiBuffer)
{
    if (plugin == nullptr)
        return;

    if (bypassed.load())
    {
        // Keep plugin processing but discard output
        bypassBuffer.makeCopyOf(buffer);
        startCPUMeasurement();
        plugin->processBlock(bypassBuffer, midiBuffer);
        endCPUMeasurement();
        // buffer remains unchanged
    }
    else
    {
        startCPUMeasurement();
        plugin->processBlock(buffer, midiBuffer);
        endCPUMeasurement();
    }
}

void PluginInstanceWrapper::releaseResources()
{
    if (plugin != nullptr)
    {
        plugin->releaseResources();
    }
}

//=============================================================================
bool PluginInstanceWrapper::hasEditor() const
{
    return plugin != nullptr && plugin->hasEditor();
}

juce::AudioProcessorEditor* PluginInstanceWrapper::createEditor()
{
    if (plugin != nullptr)
    {
        return plugin->createEditor();
    }
    return nullptr;
}

//=============================================================================
int PluginInstanceWrapper::getNumParameters() const
{
    if (plugin != nullptr)
    {
        return plugin->getParameters().size();
    }
    return 0;
}

float PluginInstanceWrapper::getParameter(int index) const
{
    if (plugin != nullptr && index < plugin->getParameters().size())
    {
        return plugin->getParameters()[index]->getValue();
    }
    return 0.0f;
}

void PluginInstanceWrapper::setParameter(int index, float value)
{
    if (plugin != nullptr && index < plugin->getParameters().size())
    {
        plugin->getParameters()[index]->setValue(value);
    }
}

juce::String PluginInstanceWrapper::getParameterName(int index) const
{
    if (plugin != nullptr && index < plugin->getParameters().size())
    {
        return plugin->getParameters()[index]->getName(128);
    }
    return {};
}

juce::String PluginInstanceWrapper::getParameterText(int index) const
{
    if (plugin != nullptr && index < plugin->getParameters().size())
    {
        return plugin->getParameters()[index]->getCurrentValueAsText();
    }
    return {};
}

} // namespace ARC

