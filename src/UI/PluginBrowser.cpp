/*
 * ============================================================================
 * ARC System Audio Host - Plugin Browser Implementation
 * ============================================================================
 */

#include "PluginBrowser.h"
#include "../Application.h"
#include "LookAndFeel.h"

namespace ARC
{

//=============================================================================
// PluginListItem Implementation
//=============================================================================

PluginListItem::PluginListItem(const juce::PluginDescription& desc, bool favorite)
    : description(desc), isFavorite(favorite)
{
    setSize(200, 50);
}

void PluginListItem::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2);

    // Background
    juce::Colour bgColour = Colors::backgroundPanel;
    if (isSelected)
        bgColour = Colors::accent.withAlpha(0.2f);
    else if (isHovered)
        bgColour = Colors::backgroundHover;

    g.setColour(bgColour);
    g.fillRoundedRectangle(bounds, 6.0f);

    // Border
    if (isSelected)
    {
        g.setColour(Colors::accent);
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
    }

    // Plugin name
    g.setColour(Colors::textPrimary);
    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawText(description.name, bounds.reduced(10, 5).removeFromTop(20),
               juce::Justification::centredLeft);

    // Manufacturer
    g.setColour(Colors::textSecondary);
    g.setFont(juce::Font(12.0f));
    g.drawText(description.manufacturerName,
               bounds.reduced(10, 5).removeFromBottom(18),
               juce::Justification::centredLeft);

    // Favorite star
    auto starBounds = bounds.removeFromRight(30).withSizeKeepingCentre(16, 16);
    g.setColour(isFavorite ? Colors::warning : Colors::textDim);
    g.setFont(juce::Font(16.0f));
    g.drawText(isFavorite ? juce::String::charToString(0x2605) : juce::String::charToString(0x2606),
               starBounds, juce::Justification::centred);

    // Plugin type indicator
    if (description.isInstrument)
    {
        g.setColour(Colors::accentSecondary);
        g.fillRoundedRectangle(bounds.getRight() - 55, bounds.getCentreY() - 8, 40, 16, 3.0f);
        g.setColour(Colors::textPrimary);
        g.setFont(juce::Font(10.0f));
        g.drawText("INST", bounds.getRight() - 55, bounds.getCentreY() - 8, 40, 16,
                   juce::Justification::centred);
    }
}

void PluginListItem::mouseEnter(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    isHovered = true;
    repaint();
}

void PluginListItem::mouseExit(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    isHovered = false;
    repaint();
}

void PluginListItem::mouseDown(const juce::MouseEvent& e)
{
    // Check if clicking on star
    auto starBounds = getLocalBounds().removeFromRight(30);
    if (starBounds.contains(e.getPosition()))
    {
        if (onFavoriteToggle)
            onFavoriteToggle(description);
        return;
    }

    if (onSelect)
        onSelect(description);
}

void PluginListItem::mouseDoubleClick(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    if (onDoubleClick)
        onDoubleClick(description);
}

//=============================================================================
// PluginBrowser Implementation
//=============================================================================

PluginBrowser::PluginBrowser(Application& app)
    : application(app)
{
    // Search box
    searchBox = std::make_unique<juce::TextEditor>();
    searchBox->setTextToShowWhenEmpty("Search plugins...", Colors::textDim);
    searchBox->addListener(this);
    addAndMakeVisible(searchBox.get());

    // Category combo
    categoryCombo = std::make_unique<juce::ComboBox>();
    categoryCombo->addItem("All", 1);
    categoryCombo->addItem("Effects", 2);
    categoryCombo->addItem("Instruments", 3);
    categoryCombo->addItem("Room Correction", 4);
    categoryCombo->setSelectedId(1);
    categoryCombo->onChange = [this]()
    {
        switch (categoryCombo->getSelectedId())
        {
            case 1: setCategory(""); break;
            case 2: setCategory("Effect"); break;
            case 3: setCategory("Instrument"); break;
            case 4: setCategory("ARC"); break;
        }
    };
    addAndMakeVisible(categoryCombo.get());

    // Favorites toggle
    favoritesOnly = std::make_unique<juce::ToggleButton>("Favorites");
    favoritesOnly->onClick = [this]()
    {
        showFavoritesOnly = favoritesOnly->getToggleState();
        updateFilteredList();
    };
    addAndMakeVisible(favoritesOnly.get());

    // Scan button
    scanButton = std::make_unique<juce::TextButton>("Rescan");
    scanButton->onClick = [this]()
    {
        isScanning = true;
        scanProgress = 0.0f;
        scanStatus = "Scanning...";
        repaint();

        application.getVSTScanner().setProgressCallback([this](const ScanProgress& progress)
        {
            scanProgress = progress.overallProgress;
            scanStatus = progress.currentFile;
            juce::MessageManager::callAsync([this]() { repaint(); });
        });

        application.getVSTScanner().setCompletionCallback([this](bool success)
        {
            juce::ignoreUnused(success);
            isScanning = false;
            juce::MessageManager::callAsync([this]()
            {
                refresh();
                repaint();
            });
        });

        application.getVSTScanner().startBackgroundScan();
    };
    addAndMakeVisible(scanButton.get());

    // Viewport for scrolling
    viewport = std::make_unique<juce::Viewport>();
    listContainer = std::make_unique<juce::Component>();
    viewport->setViewedComponent(listContainer.get(), false);
    viewport->setScrollBarsShown(true, false);
    addAndMakeVisible(viewport.get());

    refresh();
    startTimerHz(10);
}

PluginBrowser::~PluginBrowser()
{
    stopTimer();
}

void PluginBrowser::paint(juce::Graphics& g)
{
    g.setColour(Colors::backgroundPanel);
    g.fillRect(getLocalBounds());

    g.setColour(Colors::border);
    g.drawLine(getWidth() - 0.5f, 0, getWidth() - 0.5f, getHeight());

    // Title
    g.setColour(Colors::textPrimary);
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.drawText("Plugins", 15, 10, 100, 25, juce::Justification::centredLeft);

    // Scan progress
    if (isScanning)
    {
        auto progressBounds = getLocalBounds().reduced(10).removeFromBottom(30);
        g.setColour(Colors::backgroundLight);
        g.fillRoundedRectangle(progressBounds.toFloat(), 4.0f);

        g.setColour(Colors::accent);
        g.fillRoundedRectangle(progressBounds.toFloat().withWidth(
            progressBounds.getWidth() * scanProgress), 4.0f);

        g.setColour(Colors::textPrimary);
        g.setFont(12.0f);
        g.drawText("Scanning: " + juce::String(int(scanProgress * 100)) + "%",
                   progressBounds, juce::Justification::centred);
    }
}

void PluginBrowser::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    bounds.removeFromTop(35); // Title space

    searchBox->setBounds(bounds.removeFromTop(30));
    bounds.removeFromTop(8);

    auto filterRow = bounds.removeFromTop(28);
    categoryCombo->setBounds(filterRow.removeFromLeft(100));
    filterRow.removeFromLeft(10);
    favoritesOnly->setBounds(filterRow.removeFromLeft(90));
    scanButton->setBounds(filterRow.removeFromRight(70));

    bounds.removeFromTop(10);

    if (isScanning)
        bounds.removeFromBottom(40);

    viewport->setBounds(bounds);

    // Layout list items
    int y = 0;
    for (auto& item : pluginItems)
    {
        item->setBounds(0, y, viewport->getWidth() - 10, 54);
        y += 56;
    }
    listContainer->setSize(viewport->getWidth() - 10, y);
}

void PluginBrowser::textEditorTextChanged(juce::TextEditor& editor)
{
    if (&editor == searchBox.get())
    {
        filterPlugins(searchBox->getText());
    }
}

void PluginBrowser::timerCallback()
{
    // Update any dynamic content
}

void PluginBrowser::refresh()
{
    populatePluginList();
    updateFilteredList();
}

void PluginBrowser::filterPlugins(const juce::String& searchText)
{
    currentSearchText = searchText;
    updateFilteredList();
}

void PluginBrowser::setCategory(const juce::String& category)
{
    currentCategory = category;
    updateFilteredList();
}

void PluginBrowser::populatePluginList()
{
    allPlugins.clear();

    auto& scanner = application.getVSTScanner();
    allPlugins = scanner.getValidPlugins();
}

void PluginBrowser::updateFilteredList()
{
    filteredPlugins.clear();

    auto& scanner = application.getVSTScanner();

    for (const auto& plugin : allPlugins)
    {
        // Search filter
        if (currentSearchText.isNotEmpty())
        {
            if (!plugin.name.containsIgnoreCase(currentSearchText) &&
                !plugin.manufacturerName.containsIgnoreCase(currentSearchText))
            {
                continue;
            }
        }

        // Category filter
        if (currentCategory.isNotEmpty())
        {
            if (currentCategory == "Effect" && plugin.isInstrument)
                continue;
            if (currentCategory == "Instrument" && !plugin.isInstrument)
                continue;
            if (currentCategory == "ARC" &&
                !plugin.name.containsIgnoreCase("ARC"))
                continue;
        }

        // Favorites filter
        if (showFavoritesOnly)
        {
            if (!scanner.isFavorite(plugin.createIdentifierString()))
                continue;
        }

        filteredPlugins.push_back(plugin);
    }

    // Sort alphabetically
    std::sort(filteredPlugins.begin(), filteredPlugins.end(),
              [](const auto& a, const auto& b)
    {
        return a.name.compareIgnoreCase(b.name) < 0;
    });

    // Rebuild list items
    pluginItems.clear();
    listContainer->removeAllChildren();

    for (const auto& plugin : filteredPlugins)
    {
        bool isFav = scanner.isFavorite(plugin.createIdentifierString());
        auto item = std::make_unique<PluginListItem>(plugin, isFav);

        item->onSelect = [this](const juce::PluginDescription& desc)
        {
            selectPlugin(desc);
        };

        item->onDoubleClick = [this](const juce::PluginDescription& desc)
        {
            addPluginToChain(desc);
        };

        item->onFavoriteToggle = [this, &scanner](const juce::PluginDescription& desc)
        {
            auto id = desc.createIdentifierString();
            if (scanner.isFavorite(id))
                scanner.removeFromFavorites(id);
            else
                scanner.addToFavorites(id);

            updateFilteredList();
        };

        listContainer->addAndMakeVisible(item.get());
        pluginItems.push_back(std::move(item));
    }

    resized();
}

void PluginBrowser::selectPlugin(const juce::PluginDescription& desc)
{
    selectedPlugin = desc;

    for (auto& item : pluginItems)
    {
        item->setSelected(item->getDescription().createIdentifierString() ==
                         desc.createIdentifierString());
    }

    if (onPluginSelected)
        onPluginSelected(desc);
}

void PluginBrowser::addPluginToChain(const juce::PluginDescription& desc)
{
    application.getVSTChain().addPluginAsync(desc);

    if (onPluginDoubleClicked)
        onPluginDoubleClicked(desc);
}

} // namespace ARC

