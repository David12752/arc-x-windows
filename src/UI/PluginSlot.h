/*
 * ============================================================================
 * ARC System Audio Host - Plugin Slot Header
 * Visual representation of a single plugin slot in the chain
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include <functional>

namespace ARC
{

class Application;

//=============================================================================
/**
 * Plugin slot component
 * Represents a single slot in the plugin chain with controls
 */
class PluginSlotComponent : public juce::Component,
                            public juce::DragAndDropTarget
{
public:
    //=========================================================================
    PluginSlotComponent(Application& app, int slotIndex);
    ~PluginSlotComponent() override;

    //=========================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    //=========================================================================
    // DragAndDropTarget interface
    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDropped(const SourceDetails& details) override;

    //=========================================================================
    int getSlotIndex() const { return slotIndex; }
    bool isEmpty() const;

    //=========================================================================
    void updateDisplay();

    //=========================================================================
    // Callbacks
    std::function<void(int)> onOpenEditor;
    std::function<void(int)> onRemove;
    std::function<void(int)> onBypassToggle;

private:
    //=========================================================================
    void showContextMenu();

    //=========================================================================
    Application& application;
    int slotIndex;

    bool isHovered{false};
    bool isDragging{false};

    std::unique_ptr<juce::TextButton> bypassButton;
    std::unique_ptr<juce::TextButton> removeButton;
    std::unique_ptr<juce::TextButton> editButton;

    // Cached plugin info
    juce::String pluginName;
    bool pluginLoaded{false};
    bool pluginBypassed{false};
    double cpuUsage{0.0};
    bool highCPU{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginSlotComponent)
};

} // namespace ARC

