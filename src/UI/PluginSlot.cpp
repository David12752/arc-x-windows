/*
 * ============================================================================
 * ARC System Audio Host - Plugin Slot Implementation
 * ============================================================================
 */

#include "PluginSlot.h"
#include "../Application.h"
#include "LookAndFeel.h"

namespace ARC
{

//=============================================================================
PluginSlotComponent::PluginSlotComponent(Application& app, int index)
    : application(app), slotIndex(index)
{
    // Bypass button
    bypassButton = std::make_unique<juce::TextButton>("B");
    bypassButton->setClickingTogglesState(true);
    bypassButton->onClick = [this]()
    {
        if (onBypassToggle)
            onBypassToggle(slotIndex);
    };
    bypassButton->setTooltip("Bypass plugin");
    addAndMakeVisible(bypassButton.get());

    // Edit button
    editButton = std::make_unique<juce::TextButton>("E");
    editButton->onClick = [this]()
    {
        if (onOpenEditor)
            onOpenEditor(slotIndex);
    };
    editButton->setTooltip("Open plugin editor");
    addAndMakeVisible(editButton.get());

    // Remove button
    removeButton = std::make_unique<juce::TextButton>("X");
    removeButton->onClick = [this]()
    {
        if (onRemove)
            onRemove(slotIndex);
    };
    removeButton->setTooltip("Remove plugin");
    addAndMakeVisible(removeButton.get());

    updateDisplay();
}

PluginSlotComponent::~PluginSlotComponent() = default;

void PluginSlotComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2);

    // Background color based on state
    juce::Colour bgColour;

    if (isEmpty())
    {
        bgColour = Colors::backgroundPanel.withAlpha(0.5f);
    }
    else if (pluginBypassed)
    {
        bgColour = Colors::bypassed;
    }
    else if (highCPU)
    {
        bgColour = Colors::highCPU.withAlpha(0.3f);
    }
    else
    {
        bgColour = Colors::backgroundLight;
    }

    if (isHovered)
    {
        bgColour = bgColour.brighter(0.1f);
    }

    // Draw background
    g.setColour(bgColour);
    g.fillRoundedRectangle(bounds, 8.0f);

    // Draw border
    juce::Colour borderColour = isEmpty() ? Colors::border.withAlpha(0.5f) :
                                (pluginBypassed ? Colors::bypassed : Colors::accent);
    g.setColour(borderColour);
    g.drawRoundedRectangle(bounds, 8.0f, 1.5f);

    // Draw slot number
    g.setColour(Colors::textDim);
    g.setFont(juce::Font(10.0f));
    g.drawText(juce::String(slotIndex + 1), bounds.reduced(6).removeFromTop(14),
               juce::Justification::topLeft);

    if (isEmpty())
    {
        // Empty slot indicator
        g.setColour(Colors::textDim);
        g.setFont(juce::Font(12.0f));
        g.drawText("Drop plugin here", bounds, juce::Justification::centred);
    }
    else
    {
        // Plugin name
        g.setColour(Colors::textPrimary);
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        auto textBounds = bounds.reduced(8).withTrimmedTop(12).withTrimmedBottom(25);
        g.drawText(pluginName, textBounds, juce::Justification::centredLeft, true);

        // CPU usage
        g.setColour(highCPU ? Colors::highCPU : Colors::textSecondary);
        g.setFont(juce::Font(10.0f));
        g.drawText(juce::String(cpuUsage, 1) + "%",
                   bounds.reduced(6).removeFromTop(14),
                   juce::Justification::topRight);

        // Status indicator
        auto statusBounds = bounds.removeFromLeft(6).reduced(0, 20);
        g.setColour(pluginBypassed ? Colors::bypassed : Colors::active);
        g.fillRoundedRectangle(statusBounds, 3.0f);
    }
}

void PluginSlotComponent::resized()
{
    auto bounds = getLocalBounds().reduced(8);
    auto buttonRow = bounds.removeFromBottom(22);

    int buttonWidth = 24;
    int spacing = 4;

    bypassButton->setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(spacing);
    editButton->setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(spacing);
    removeButton->setBounds(buttonRow.removeFromRight(buttonWidth));

    // Hide buttons for empty slots
    bool hasPlugin = !isEmpty();
    bypassButton->setVisible(hasPlugin);
    editButton->setVisible(hasPlugin);
    removeButton->setVisible(hasPlugin);
}

void PluginSlotComponent::mouseEnter(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    isHovered = true;
    repaint();
}

void PluginSlotComponent::mouseExit(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    isHovered = false;
    repaint();
}

void PluginSlotComponent::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        showContextMenu();
    }
}

void PluginSlotComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    if (!isEmpty() && onOpenEditor)
    {
        onOpenEditor(slotIndex);
    }
}

void PluginSlotComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (!isEmpty() && e.getDistanceFromDragStart() > 5)
    {
        isDragging = true;

        // Start drag and drop
        if (auto* dragContainer = juce::DragAndDropContainer::findParentDragContainerFor(this))
        {
            juce::Image dragImage(juce::Image::ARGB, getWidth(), getHeight(), true);
            juce::Graphics g(dragImage);
            paint(g);
            dragImage.multiplyAllAlphas(0.7f);

            dragContainer->startDragging("plugin_slot", this, juce::ScaledImage(dragImage));
        }
    }
}

bool PluginSlotComponent::isInterestedInDragSource(const SourceDetails& details)
{
    return !details.description.isVoid();
}

void PluginSlotComponent::itemDropped(const SourceDetails& details)
{
    juce::ignoreUnused(details);
    // Handle drop in parent component
}

bool PluginSlotComponent::isEmpty() const
{
    return !pluginLoaded;
}

void PluginSlotComponent::updateDisplay()
{
    auto& chain = application.getVSTChain();
    auto* plugin = chain.getPlugin(slotIndex);

    if (plugin != nullptr)
    {
        pluginLoaded = true;
        pluginName = plugin->getName();
        pluginBypassed = plugin->isBypassed();
        cpuUsage = plugin->getCPUInfo().currentUsage;
        highCPU = cpuUsage > 20.0;
    }
    else
    {
        pluginLoaded = false;
        pluginName = "";
        pluginBypassed = false;
        cpuUsage = 0.0;
        highCPU = false;
    }

    // Update bypass button state
    bypassButton->setToggleState(pluginBypassed, juce::dontSendNotification);

    resized();
    repaint();
}

void PluginSlotComponent::showContextMenu()
{
    juce::PopupMenu menu;

    if (!isEmpty())
    {
        menu.addItem(1, "Open Editor", true, false);
        menu.addItem(2, pluginBypassed ? "Enable" : "Bypass", true, false);
        menu.addSeparator();
        menu.addItem(3, "Move Up", slotIndex > 0, false);
        menu.addItem(4, "Move Down", true, false);
        menu.addSeparator();
        menu.addItem(5, "Save Preset...", true, false);
        menu.addItem(6, "Load Preset...", true, false);
        menu.addSeparator();
        menu.addItem(7, "Remove", true, false);
    }
    else
    {
        menu.addItem(10, "Paste Plugin", false, false);
    }

    menu.showMenuAsync(juce::PopupMenu::Options(), [this](int result)
    {
        switch (result)
        {
            case 1:
                if (onOpenEditor) onOpenEditor(slotIndex);
                break;
            case 2:
                if (onBypassToggle) onBypassToggle(slotIndex);
                break;
            case 3:
                application.getVSTChain().movePlugin(slotIndex, slotIndex - 1);
                break;
            case 4:
                application.getVSTChain().movePlugin(slotIndex, slotIndex + 1);
                break;
            case 5:
                // Save preset
                {
                    auto* plugin = application.getVSTChain().getPlugin(slotIndex);
                    if (plugin != nullptr)
                    {
                        juce::FileChooser chooser("Save Plugin Preset",
                                                  juce::File::getSpecialLocation(
                                                      juce::File::userDocumentsDirectory),
                                                  "*.vstpreset");

                        if (chooser.browseForFileToSave(true))
                        {
                            plugin->saveStateToFile(chooser.getResult());
                        }
                    }
                }
                break;
            case 6:
                // Load preset
                {
                    auto* plugin = application.getVSTChain().getPlugin(slotIndex);
                    if (plugin != nullptr)
                    {
                        juce::FileChooser chooser("Load Plugin Preset",
                                                  juce::File::getSpecialLocation(
                                                      juce::File::userDocumentsDirectory),
                                                  "*.vstpreset");

                        if (chooser.browseForFileToOpen())
                        {
                            plugin->loadStateFromFile(chooser.getResult());
                        }
                    }
                }
                break;
            case 7:
                if (onRemove) onRemove(slotIndex);
                break;
        }
    });
}

} // namespace ARC

