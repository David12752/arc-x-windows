/*
 * ============================================================================
 * ARC System Audio Host - Preset Manager Header
 * Manages saving and loading of plugin chain presets
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include <vector>
#include <functional>

namespace ARC
{

class VSTChain;

//=============================================================================
/**
 * Preset metadata
 */
struct PresetInfo
{
    juce::String name;
    juce::String author;
    juce::String description;
    juce::String category;
    juce::Time createdTime;
    juce::Time modifiedTime;
    juce::File file;
    bool isFavorite{false};
    bool isFactory{false};
    int version{1};
};

//=============================================================================
/**
 * Preset manager class
 * Handles saving, loading, and organizing presets
 */
class PresetManager
{
public:
    //=========================================================================
    using PresetChangeCallback = std::function<void()>;

    //=========================================================================
    PresetManager();
    ~PresetManager();

    //=========================================================================
    // Initialization
    void initialize(const juce::File& presetDirectory);
    void setChain(VSTChain* chain);

    //=========================================================================
    // Preset operations
    bool savePreset(const juce::String& name, const juce::String& category = "User");
    bool savePresetAs(const juce::File& file);
    bool loadPreset(const juce::File& file);
    bool loadPreset(const PresetInfo& preset);
    bool deletePreset(const PresetInfo& preset);
    bool renamePreset(const PresetInfo& preset, const juce::String& newName);

    //=========================================================================
    // Current preset
    juce::String getCurrentPresetName() const;
    juce::File getCurrentPresetFile() const;
    bool isCurrentPresetModified() const;
    void markCurrentPresetAsModified();

    //=========================================================================
    // Preset enumeration
    std::vector<PresetInfo> getAllPresets() const;
    std::vector<PresetInfo> getPresetsInCategory(const juce::String& category) const;
    std::vector<PresetInfo> getFavoritePresets() const;
    std::vector<PresetInfo> getRecentPresets(int maxCount = 10) const;
    juce::StringArray getCategories() const;

    //=========================================================================
    // Search
    std::vector<PresetInfo> searchPresets(const juce::String& searchText) const;

    //=========================================================================
    // Favorites
    void addToFavorites(const PresetInfo& preset);
    void removeFromFavorites(const PresetInfo& preset);
    bool isFavorite(const PresetInfo& preset) const;

    //=========================================================================
    // Preset banks
    bool exportBank(const juce::File& file, const std::vector<PresetInfo>& presets);
    bool importBank(const juce::File& file);

    //=========================================================================
    // Quick slots (for instant preset switching)
    static constexpr int NUM_QUICK_SLOTS = 8;

    void assignToQuickSlot(int slot, const PresetInfo& preset);
    PresetInfo getQuickSlot(int slot) const;
    void clearQuickSlot(int slot);
    void loadQuickSlot(int slot);

    //=========================================================================
    // Preset directories
    juce::File getPresetDirectory() const;
    juce::File getUserPresetDirectory() const;
    juce::File getFactoryPresetDirectory() const;

    //=========================================================================
    // Callbacks
    void addPresetChangeCallback(PresetChangeCallback callback);
    void notifyPresetChanged();

    //=========================================================================
    // Persistence
    void saveState();
    void loadState();

private:
    //=========================================================================
    void scanPresets();
    void addRecent(const PresetInfo& preset);
    PresetInfo createPresetInfo(const juce::File& file) const;
    bool writePresetFile(const juce::File& file, const PresetInfo& info);
    bool readPresetFile(const juce::File& file, PresetInfo& info);

    //=========================================================================
    VSTChain* vstChain{nullptr};

    juce::File presetDirectory;
    juce::File userDirectory;
    juce::File factoryDirectory;

    std::vector<PresetInfo> presets;
    std::vector<PresetInfo> recentPresets;
    juce::StringArray favoriteIds;
    std::array<PresetInfo, NUM_QUICK_SLOTS> quickSlots;

    juce::String currentPresetName;
    juce::File currentPresetFile;
    bool currentPresetModified{false};

    std::vector<PresetChangeCallback> callbacks;
    mutable std::mutex callbackMutex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};

} // namespace ARC

