/*
 * ============================================================================
 * ARC System Audio Host - Preset Manager Implementation
 * ============================================================================
 */

#include "PresetManager.h"
#include "../VST/VSTChain.h"
#include "Logger.h"
#include "Constants.h"

namespace ARC
{

//=============================================================================
PresetManager::PresetManager()
{
    // Set default preset directory
    presetDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                          .getChildFile("ARC System Audio Host")
                          .getChildFile("Presets");

    userDirectory = presetDirectory.getChildFile("User");
    factoryDirectory = presetDirectory.getChildFile("Factory");

    // Create directories if they don't exist
    presetDirectory.createDirectory();
    userDirectory.createDirectory();
    factoryDirectory.createDirectory();

    Logger::log("PresetManager created - directory: " + presetDirectory.getFullPathName());
}

PresetManager::~PresetManager()
{
    saveState();
    Logger::log("PresetManager destroyed");
}

//=============================================================================
void PresetManager::initialize(const juce::File& directory)
{
    presetDirectory = directory;
    userDirectory = directory.getChildFile("User");
    factoryDirectory = directory.getChildFile("Factory");

    presetDirectory.createDirectory();
    userDirectory.createDirectory();
    factoryDirectory.createDirectory();

    scanPresets();
    loadState();
}

void PresetManager::setChain(VSTChain* chain)
{
    vstChain = chain;
}

//=============================================================================
bool PresetManager::savePreset(const juce::String& name, const juce::String& category)
{
    if (vstChain == nullptr)
        return false;

    juce::File categoryDir = userDirectory.getChildFile(category);
    categoryDir.createDirectory();

    juce::File presetFile = categoryDir.getChildFile(name + ".arcpreset");

    PresetInfo info;
    info.name = name;
    info.category = category;
    info.createdTime = juce::Time::getCurrentTime();
    info.modifiedTime = info.createdTime;
    info.file = presetFile;
    info.isFactory = false;

    if (writePresetFile(presetFile, info))
    {
        vstChain->saveState(presetFile);
        currentPresetName = name;
        currentPresetFile = presetFile;
        currentPresetModified = false;

        scanPresets();
        addRecent(info);
        notifyPresetChanged();

        Logger::log("Preset saved: " + name);
        return true;
    }

    return false;
}

bool PresetManager::savePresetAs(const juce::File& file)
{
    if (vstChain == nullptr)
    {
        Logger::error("Cannot save preset: VSTChain is null");
        return false;
    }
    
    // Path validation
    juce::String canonicalPath = file.getFullPathName();
    
    // Check for path traversal attempts
    if (canonicalPath.contains("..") || canonicalPath.contains("~"))
    {
        Logger::error("Invalid preset path (path traversal detected): " + canonicalPath);
        return false;
    }
    
    // Ensure file has correct extension
    if (!file.hasFileExtension("arcpreset"))
    {
        Logger::error("Preset file must have .arcpreset extension");
        return false;
    }

    PresetInfo info;
    info.name = file.getFileNameWithoutExtension();
    info.category = "User";
    info.createdTime = juce::Time::getCurrentTime();
    info.modifiedTime = info.createdTime;
    info.file = file;

    if (writePresetFile(file, info))
    {
        vstChain->saveState(file);
        currentPresetName = info.name;
        currentPresetFile = file;
        currentPresetModified = false;

        scanPresets();
        addRecent(info);
        notifyPresetChanged();

        Logger::log("Preset saved as: " + file.getFullPathName());
        return true;
    }

    return false;
}

bool PresetManager::loadPreset(const juce::File& file)
{
    if (vstChain == nullptr)
    {
        Logger::error("Cannot load preset: VSTChain is null");
        return false;
    }

    // Path validation - prevent path traversal attacks
    juce::String canonicalPath = file.getFullPathName();
    
    // Check for path traversal attempts
    if (canonicalPath.contains("..") || canonicalPath.contains("~"))
    {
        Logger::error("Invalid preset path (path traversal detected): " + canonicalPath);
        return false;
    }
    
    // Check file extension
    if (!file.hasFileExtension("arcpreset"))
    {
        Logger::error("Invalid preset file extension: " + file.getFileExtension());
        return false;
    }
    
    // Check file exists
    if (!file.existsAsFile())
    {
        Logger::error("Preset file does not exist: " + canonicalPath);
        return false;
    }
    
    // Check file size (prevent DoS with huge files)
    constexpr int64_t MAX_PRESET_SIZE = 10 * 1024 * 1024; // 10 MB
    if (file.getSize() > MAX_PRESET_SIZE)
    {
        Logger::error("Preset file too large: " + juce::String(file.getSize()) + " bytes");
        return false;
    }
    
    // Verify file is within preset directory (security check)
    if (!file.isAChildOf(presetDirectory))
    {
        Logger::warning("Loading preset from outside preset directory: " + canonicalPath);
        // Allow but log warning
    }

    Logger::log("Loading preset: " + file.getFullPathName());

    if (vstChain->loadState(file))
    {
        PresetInfo info = createPresetInfo(file);
        currentPresetName = info.name;
        currentPresetFile = file;
        currentPresetModified = false;

        addRecent(info);
        notifyPresetChanged();

        Logger::log("Preset loaded: " + info.name);
        return true;
    }

    Logger::log("Failed to load preset: " + file.getFullPathName());
    return false;
}

bool PresetManager::loadPreset(const PresetInfo& preset)
{
    return loadPreset(preset.file);
}

bool PresetManager::deletePreset(const PresetInfo& preset)
{
    if (preset.isFactory)
    {
        Logger::log("Cannot delete factory preset: " + preset.name);
        return false;
    }

    if (preset.file.deleteFile())
    {
        scanPresets();
        notifyPresetChanged();
        Logger::log("Preset deleted: " + preset.name);
        return true;
    }

    return false;
}

bool PresetManager::renamePreset(const PresetInfo& preset, const juce::String& newName)
{
    if (preset.isFactory)
    {
        Logger::log("Cannot rename factory preset: " + preset.name);
        return false;
    }

    juce::File newFile = preset.file.getParentDirectory().getChildFile(newName + ".arcpreset");

    if (preset.file.moveFileTo(newFile))
    {
        scanPresets();
        notifyPresetChanged();
        Logger::log("Preset renamed: " + preset.name + " -> " + newName);
        return true;
    }

    return false;
}

//=============================================================================
juce::String PresetManager::getCurrentPresetName() const
{
    return currentPresetName;
}

juce::File PresetManager::getCurrentPresetFile() const
{
    return currentPresetFile;
}

bool PresetManager::isCurrentPresetModified() const
{
    return currentPresetModified;
}

void PresetManager::markCurrentPresetAsModified()
{
    currentPresetModified = true;
    notifyPresetChanged();
}

//=============================================================================
std::vector<PresetInfo> PresetManager::getAllPresets() const
{
    return presets;
}

std::vector<PresetInfo> PresetManager::getPresetsInCategory(const juce::String& category) const
{
    std::vector<PresetInfo> result;

    for (const auto& preset : presets)
    {
        if (preset.category.equalsIgnoreCase(category))
        {
            result.push_back(preset);
        }
    }

    return result;
}

std::vector<PresetInfo> PresetManager::getFavoritePresets() const
{
    std::vector<PresetInfo> result;

    for (const auto& preset : presets)
    {
        if (preset.isFavorite)
        {
            result.push_back(preset);
        }
    }

    return result;
}

std::vector<PresetInfo> PresetManager::getRecentPresets(int maxCount) const
{
    std::vector<PresetInfo> result;
    int count = juce::jmin(maxCount, static_cast<int>(recentPresets.size()));

    for (int i = 0; i < count; ++i)
    {
        result.push_back(recentPresets[i]);
    }

    return result;
}

juce::StringArray PresetManager::getCategories() const
{
    juce::StringArray categories;

    for (const auto& preset : presets)
    {
        if (!categories.contains(preset.category))
        {
            categories.add(preset.category);
        }
    }

    categories.sort(true);
    return categories;
}

//=============================================================================
std::vector<PresetInfo> PresetManager::searchPresets(const juce::String& searchText) const
{
    std::vector<PresetInfo> result;

    for (const auto& preset : presets)
    {
        if (preset.name.containsIgnoreCase(searchText) ||
            preset.author.containsIgnoreCase(searchText) ||
            preset.category.containsIgnoreCase(searchText) ||
            preset.description.containsIgnoreCase(searchText))
        {
            result.push_back(preset);
        }
    }

    return result;
}

//=============================================================================
void PresetManager::addToFavorites(const PresetInfo& preset)
{
    juce::String id = preset.file.getFullPathName();
    if (!favoriteIds.contains(id))
    {
        favoriteIds.add(id);
        scanPresets(); // Refresh to update favorite flags
        saveState();
    }
}

void PresetManager::removeFromFavorites(const PresetInfo& preset)
{
    juce::String id = preset.file.getFullPathName();
    favoriteIds.removeString(id);
    scanPresets();
    saveState();
}

bool PresetManager::isFavorite(const PresetInfo& preset) const
{
    return favoriteIds.contains(preset.file.getFullPathName());
}

//=============================================================================
bool PresetManager::exportBank(const juce::File& file, const std::vector<PresetInfo>& bankPresets)
{
    juce::ZipFile::Builder builder;

    for (const auto& preset : bankPresets)
    {
        if (preset.file.existsAsFile())
        {
            builder.addFile(preset.file, 9, preset.file.getFileName());
        }
    }

    juce::FileOutputStream output(file);
    if (output.openedOk())
    {
        builder.writeToStream(output, nullptr);
        Logger::log("Bank exported: " + file.getFullPathName());
        return true;
    }

    return false;
}

bool PresetManager::importBank(const juce::File& file)
{
    juce::ZipFile zip(file);

    if (zip.getNumEntries() == 0)
        return false;

    juce::File importDir = userDirectory.getChildFile("Imported");
    importDir.createDirectory();

    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        auto* entry = zip.getEntry(i);
        if (entry != nullptr)
        {
            juce::File destFile = importDir.getChildFile(entry->filename);
            zip.uncompressEntry(i, destFile);
        }
    }

    scanPresets();
    notifyPresetChanged();

    Logger::log("Bank imported: " + file.getFullPathName());
    return true;
}

//=============================================================================
void PresetManager::assignToQuickSlot(int slot, const PresetInfo& preset)
{
    if (slot >= 0 && slot < NUM_QUICK_SLOTS)
    {
        quickSlots[slot] = preset;
        saveState();
    }
}

PresetInfo PresetManager::getQuickSlot(int slot) const
{
    if (slot >= 0 && slot < NUM_QUICK_SLOTS)
    {
        return quickSlots[slot];
    }
    return PresetInfo{};
}

void PresetManager::clearQuickSlot(int slot)
{
    if (slot >= 0 && slot < NUM_QUICK_SLOTS)
    {
        quickSlots[slot] = PresetInfo{};
        saveState();
    }
}

void PresetManager::loadQuickSlot(int slot)
{
    if (slot >= 0 && slot < NUM_QUICK_SLOTS)
    {
        const auto& preset = quickSlots[slot];
        if (preset.file.existsAsFile())
        {
            loadPreset(preset);
        }
    }
}

//=============================================================================
juce::File PresetManager::getPresetDirectory() const
{
    return presetDirectory;
}

juce::File PresetManager::getUserPresetDirectory() const
{
    return userDirectory;
}

juce::File PresetManager::getFactoryPresetDirectory() const
{
    return factoryDirectory;
}

//=============================================================================
void PresetManager::addPresetChangeCallback(PresetChangeCallback callback)
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    callbacks.push_back(callback);
}

void PresetManager::notifyPresetChanged()
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    for (auto& callback : callbacks)
    {
        callback();
    }
}

//=============================================================================
void PresetManager::saveState()
{
    juce::File stateFile = presetDirectory.getChildFile("presetmanager.state");

    auto xml = std::make_unique<juce::XmlElement>("PresetManagerState");

    // Save favorites
    auto* favXml = xml->createNewChildElement("Favorites");
    for (const auto& id : favoriteIds)
    {
        favXml->createNewChildElement("Item")->setAttribute("path", id);
    }

    // Save recent
    auto* recentXml = xml->createNewChildElement("Recent");
    for (const auto& preset : recentPresets)
    {
        recentXml->createNewChildElement("Item")->setAttribute("path",
            preset.file.getFullPathName());
    }

    // Save quick slots
    auto* slotsXml = xml->createNewChildElement("QuickSlots");
    for (int i = 0; i < NUM_QUICK_SLOTS; ++i)
    {
        auto* slot = slotsXml->createNewChildElement("Slot");
        slot->setAttribute("index", i);
        slot->setAttribute("path", quickSlots[i].file.getFullPathName());
    }

    xml->writeTo(stateFile);
}

void PresetManager::loadState()
{
    juce::File stateFile = presetDirectory.getChildFile("presetmanager.state");

    if (!stateFile.existsAsFile())
        return;

    auto xml = juce::XmlDocument::parse(stateFile);
    if (xml == nullptr)
        return;

    // Load favorites
    if (auto* favXml = xml->getChildByName("Favorites"))
    {
        favoriteIds.clear();
        for (auto* item : favXml->getChildIterator())
        {
            favoriteIds.add(item->getStringAttribute("path"));
        }
    }

    // Load recent
    if (auto* recentXml = xml->getChildByName("Recent"))
    {
        recentPresets.clear();
        for (auto* item : recentXml->getChildIterator())
        {
            juce::File file(item->getStringAttribute("path"));
            if (file.existsAsFile())
            {
                recentPresets.push_back(createPresetInfo(file));
            }
        }
    }

    // Load quick slots
    if (auto* slotsXml = xml->getChildByName("QuickSlots"))
    {
        for (auto* slot : slotsXml->getChildIterator())
        {
            int index = slot->getIntAttribute("index");
            juce::File file(slot->getStringAttribute("path"));

            if (index >= 0 && index < NUM_QUICK_SLOTS && file.existsAsFile())
            {
                quickSlots[index] = createPresetInfo(file);
            }
        }
    }

    // Refresh presets with favorite flags
    scanPresets();
}

//=============================================================================
void PresetManager::scanPresets()
{
    presets.clear();

    // Scan factory presets
    auto factoryFiles = factoryDirectory.findChildFiles(
        juce::File::findFiles, true, "*.arcpreset");

    for (const auto& file : factoryFiles)
    {
        PresetInfo info = createPresetInfo(file);
        info.isFactory = true;
        info.isFavorite = favoriteIds.contains(file.getFullPathName());
        presets.push_back(info);
    }

    // Scan user presets
    auto userFiles = userDirectory.findChildFiles(
        juce::File::findFiles, true, "*.arcpreset");

    for (const auto& file : userFiles)
    {
        PresetInfo info = createPresetInfo(file);
        info.isFactory = false;
        info.isFavorite = favoriteIds.contains(file.getFullPathName());
        presets.push_back(info);
    }

    // Sort by name
    std::sort(presets.begin(), presets.end(),
              [](const PresetInfo& a, const PresetInfo& b)
    {
        return a.name.compareIgnoreCase(b.name) < 0;
    });

    Logger::log("Scanned " + juce::String(presets.size()) + " presets");
}

void PresetManager::addRecent(const PresetInfo& preset)
{
    // Remove if already in list
    recentPresets.erase(
        std::remove_if(recentPresets.begin(), recentPresets.end(),
                       [&](const PresetInfo& p) {
                           return p.file == preset.file;
                       }),
        recentPresets.end());

    // Add to front
    recentPresets.insert(recentPresets.begin(), preset);

    // Limit size
    if (recentPresets.size() > 20)
    {
        recentPresets.resize(20);
    }

    saveState();
}

PresetInfo PresetManager::createPresetInfo(const juce::File& file) const
{
    PresetInfo info;
    info.name = file.getFileNameWithoutExtension();
    info.file = file;
    info.modifiedTime = file.getLastModificationTime();
    info.createdTime = file.getCreationTime();

    // Get category from parent folder
    juce::File parent = file.getParentDirectory();
    if (parent != userDirectory && parent != factoryDirectory)
    {
        info.category = parent.getFileName();
    }
    else
    {
        info.category = parent == factoryDirectory ? "Factory" : "User";
    }

    return info;
}

bool PresetManager::writePresetFile(const juce::File& file, const PresetInfo& info)
{
    // The actual preset data is written by VSTChain::saveState
    // This function writes metadata

    juce::File metaFile = file.withFileExtension(".meta");

    auto xml = std::make_unique<juce::XmlElement>("PresetMeta");
    xml->setAttribute("name", info.name);
    xml->setAttribute("author", info.author);
    xml->setAttribute("description", info.description);
    xml->setAttribute("category", info.category);
    xml->setAttribute("version", info.version);

    return xml->writeTo(metaFile);
}

bool PresetManager::readPresetFile(const juce::File& file, PresetInfo& info)
{
    juce::File metaFile = file.withFileExtension(".meta");

    if (metaFile.existsAsFile())
    {
        auto xml = juce::XmlDocument::parse(metaFile);
        if (xml != nullptr)
        {
            info.name = xml->getStringAttribute("name", file.getFileNameWithoutExtension());
            info.author = xml->getStringAttribute("author");
            info.description = xml->getStringAttribute("description");
            info.category = xml->getStringAttribute("category", "User");
            info.version = xml->getIntAttribute("version", 1);
            return true;
        }
    }

    // Default info from filename
    info = createPresetInfo(file);
    return true;
}

} // namespace ARC

