/*
 * ============================================================================
 * ARC System Audio Host - Plugin Chain View Header
 * Visual representation of the plugin processing chain
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include "PluginSlot.h"
#include <vector>
#include <memory>

namespace ARC
{

class Application;

//=============================================================================
/**
 * Plugin chain view component
 * Displays the chain of plugins with drag-and-drop reordering
 */
class PluginChainView : public juce::Component,
                        public juce::DragAndDropContainer,
                        public juce::DragAndDropTarget,
                        public juce::Timer
{
public:
    //=========================================================================
    PluginChainView(Application& app);
    ~PluginChainView() override;

    //=========================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

    //=========================================================================
    // DragAndDropTarget interface
    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDragEnter(const SourceDetails& details) override;
    void itemDragMove(const SourceDetails& details) override;
    void itemDragExit(const SourceDetails& details) override;
    void itemDropped(const SourceDetails& details) override;

    //=========================================================================
    // Timer
    void timerCallback() override;

    //=========================================================================
    void refresh();
    void updateSlots();

    //=========================================================================
    // Plugin window management
    void openPluginEditor(int slotIndex);
    void closePluginEditor(int slotIndex);

private:
    //=========================================================================
    void drawSignalFlow(juce::Graphics& g);
    void drawDropIndicator(juce::Graphics& g);
    int getSlotIndexAtPosition(const juce::Point<int>& pos) const;

    //=========================================================================
    Application& application;

    std::vector<std::unique_ptr<PluginSlotComponent>> slotComponents;

    // Drag and drop state
    bool isDraggingOver{false};
    int dropTargetIndex{-1};
    juce::Point<int> dragPosition;

    // ARC specific
    std::unique_ptr<juce::TextButton> arcCompareButton;
    bool arcCompareMode{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginChainView)
};

} // namespace ARC

