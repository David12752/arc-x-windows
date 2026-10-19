/*
 * ============================================================================
 * ARC System Audio Host - Plugin Browser Header
 * Displays and manages available VST3 plugins
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include <vector>
#include <functional>

namespace ARC
{

class Application;

//=============================================================================
/**
 * Plugin list item component
 */
class PluginListItem : public juce::Component
{
public:
    PluginListItem(const juce::PluginDescription& desc, bool favorite = false);

    void paint(juce::Graphics& g) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    const juce::PluginDescription& getDescription() const { return description; }
    void setFavorite(bool fav) { isFavorite = fav; repaint(); }
    bool getFavorite() const { return isFavorite; }
    void setSelected(bool sel) { isSelected = sel; repaint(); }

    std::function<void(const juce::PluginDescription&)> onDoubleClick;
    std::function<void(const juce::PluginDescription&)> onSelect;
    std::function<void(const juce::PluginDescription&)> onFavoriteToggle;

private:
    juce::PluginDescription description;
    bool isHovered{false};
    bool isSelected{false};
    bool isFavorite{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginListItem)
};

//=============================================================================
/**
 * Plugin browser panel
 * Shows searchable list of available plugins with categories and favorites
 */
class PluginBrowser : public juce::Component,
                      public juce::TextEditor::Listener,
                      public juce::Timer
{
public:
    //=========================================================================
    PluginBrowser(Application& app);
    ~PluginBrowser() override;

    //=========================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

    //=========================================================================
    void textEditorTextChanged(juce::TextEditor& editor) override;
    void timerCallback() override;

    //=========================================================================
    void refresh();
    void filterPlugins(const juce::String& searchText);
    void setCategory(const juce::String& category);

    //=========================================================================
    // Callbacks
    std::function<void(const juce::PluginDescription&)> onPluginSelected;
    std::function<void(const juce::PluginDescription&)> onPluginDoubleClicked;

private:
    //=========================================================================
    void populatePluginList();
    void updateFilteredList();
    void selectPlugin(const juce::PluginDescription& desc);
    void addPluginToChain(const juce::PluginDescription& desc);

    //=========================================================================
    Application& application;

    std::unique_ptr<juce::TextEditor> searchBox;
    std::unique_ptr<juce::ComboBox> categoryCombo;
    std::unique_ptr<juce::ToggleButton> favoritesOnly;
    std::unique_ptr<juce::TextButton> scanButton;
    std::unique_ptr<juce::Viewport> viewport;
    std::unique_ptr<juce::Component> listContainer;

    std::vector<std::unique_ptr<PluginListItem>> pluginItems;
    std::vector<juce::PluginDescription> allPlugins;
    std::vector<juce::PluginDescription> filteredPlugins;

    juce::String currentSearchText;
    juce::String currentCategory;
    bool showFavoritesOnly{false};

    juce::PluginDescription selectedPlugin;

    // Scan progress
    bool isScanning{false};
    float scanProgress{0.0f};
    juce::String scanStatus;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginBrowser)
};

} // namespace ARC

