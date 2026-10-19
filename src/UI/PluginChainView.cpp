/*
 * ============================================================================
 * ARC System Audio Host - Plugin Chain View Implementation
 * ============================================================================
 */

#include "PluginChainView.h"
#include "../Application.h"
#include "LookAndFeel.h"

namespace ARC
{

//=============================================================================
PluginChainView::PluginChainView(Application& app)
    : application(app)
{
    // ARC A/B compare button
    arcCompareButton = std::make_unique<juce::TextButton>("A/B Compare");
    arcCompareButton->setClickingTogglesState(true);
    arcCompareButton->onClick = [this]()
    {
        arcCompareMode = arcCompareButton->getToggleState();
        application.getVSTChain().setARCComparisonMode(!arcCompareMode);
    };
    arcCompareButton->setVisible(false);
    addAndMakeVisible(arcCompareButton.get());

    // Set up chain change callback
    application.getVSTChain().setChainChangeCallback([this]()
    {
        juce::MessageManager::callAsync([this]() { updateSlots(); });
    });

    updateSlots();
    startTimerHz(15);
}

PluginChainView::~PluginChainView()
{
    stopTimer();
}

void PluginChainView::paint(juce::Graphics& g)
{
    g.fillAll(Colors::background);

    // Title
    g.setColour(Colors::textPrimary);
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.drawText("Plugin Chain", 15, 10, 200, 25, juce::Justification::centredLeft);

    // Draw signal flow arrows
    drawSignalFlow(g);

    // Draw drop indicator if dragging
    if (isDraggingOver && dropTargetIndex >= 0)
    {
        drawDropIndicator(g);
    }

    // Empty state
    if (slotComponents.empty() ||
        std::all_of(slotComponents.begin(), slotComponents.end(),
                    [](const auto& s) { return s->isEmpty(); }))
    {
        g.setColour(Colors::textDim);
        g.setFont(juce::Font(14.0f));
        g.drawText("Drag plugins here or double-click in the browser to add",
                   getLocalBounds().withTrimmedTop(50),
                   juce::Justification::centred);
    }
}

void PluginChainView::resized()
{
    auto bounds = getLocalBounds().reduced(15);
    bounds.removeFromTop(40); // Title space

    // ARC compare button
    if (application.getVSTChain().hasARCPlugin())
    {
        arcCompareButton->setVisible(true);
        arcCompareButton->setBounds(bounds.removeFromTop(30).removeFromRight(120));
        bounds.removeFromTop(10);
    }

    // Layout slots in a grid
    const int slotWidth = 200;
    const int slotHeight = 80;
    const int slotSpacing = 20;

    int slotsPerRow = juce::jmax(1, (bounds.getWidth() + slotSpacing) / (slotWidth + slotSpacing));
    int x = 0;
    int y = 0;
    int index = 0;

    for (auto& slot : slotComponents)
    {
        slot->setBounds(x * (slotWidth + slotSpacing),
                        bounds.getY() + y * (slotHeight + slotSpacing),
                        slotWidth, slotHeight);

        ++index;
        ++x;
        if (x >= slotsPerRow)
        {
            x = 0;
            ++y;
        }
    }
}

void PluginChainView::drawSignalFlow(juce::Graphics& g)
{
    if (slotComponents.size() < 2)
        return;

    g.setColour(Colors::accent.withAlpha(0.3f));

    for (size_t i = 0; i < slotComponents.size() - 1; ++i)
    {
        if (slotComponents[i]->isEmpty() && slotComponents[i + 1]->isEmpty())
            continue;

        auto& current = slotComponents[i];
        auto& next = slotComponents[i + 1];

        auto start = current->getBounds().getCentre().toFloat();
        auto end = next->getBounds().getCentre().toFloat();

        // Check if on same row
        if (std::abs(start.y - end.y) < 20)
        {
            // Horizontal arrow
            juce::Path arrow;
            float midX = (start.x + end.x) / 2;

            arrow.startNewSubPath(start.x + 100, start.y);
            arrow.lineTo(end.x - 100, end.y);

            // Arrow head
            arrow.lineTo(end.x - 110, end.y - 6);
            arrow.startNewSubPath(end.x - 100, end.y);
            arrow.lineTo(end.x - 110, end.y + 6);

            g.strokePath(arrow, juce::PathStrokeType(2.0f));
        }
        else
        {
            // Curved path for different rows
            juce::Path path;
            path.startNewSubPath(start.x + 100, start.y);

            auto ctrl1 = juce::Point<float>(start.x + 150, start.y);
            auto ctrl2 = juce::Point<float>(end.x - 150, end.y);

            path.cubicTo(ctrl1, ctrl2, juce::Point<float>(end.x - 100, end.y));

            g.strokePath(path, juce::PathStrokeType(2.0f));
        }
    }
}

void PluginChainView::drawDropIndicator(juce::Graphics& g)
{
    if (dropTargetIndex < 0 || dropTargetIndex >= static_cast<int>(slotComponents.size()))
        return;

    auto& targetSlot = slotComponents[dropTargetIndex];
    auto bounds = targetSlot->getBounds().toFloat();

    g.setColour(Colors::accent);
    g.drawRoundedRectangle(bounds.expanded(4), 8.0f, 3.0f);

    // Glow effect
    ARCLookAndFeel::drawGlowEffect(g, bounds, Colors::accent, 15.0f);
}

bool PluginChainView::isInterestedInDragSource(const SourceDetails& details)
{
    // Accept plugin descriptions or slot reordering
    return !details.description.isVoid();
}

void PluginChainView::itemDragEnter(const SourceDetails& details)
{
    juce::ignoreUnused(details);
    isDraggingOver = true;
    repaint();
}

void PluginChainView::itemDragMove(const SourceDetails& details)
{
    dragPosition = details.localPosition;
    dropTargetIndex = getSlotIndexAtPosition(dragPosition);
    repaint();
}

void PluginChainView::itemDragExit(const SourceDetails& details)
{
    juce::ignoreUnused(details);
    isDraggingOver = false;
    dropTargetIndex = -1;
    repaint();
}

void PluginChainView::itemDropped(const SourceDetails& details)
{
    isDraggingOver = false;

    if (dropTargetIndex >= 0)
    {
        // Check if this is a plugin description
        if (auto* desc = dynamic_cast<juce::PluginDescription*>(details.sourceComponent.get()))
        {
            application.getVSTChain().addPlugin(*desc, dropTargetIndex);
        }
        // Or reordering existing plugin
        else if (auto* sourceSlot = dynamic_cast<PluginSlotComponent*>(details.sourceComponent.get()))
        {
            int sourceIndex = sourceSlot->getSlotIndex();
            if (sourceIndex != dropTargetIndex)
            {
                application.getVSTChain().movePlugin(sourceIndex, dropTargetIndex);
            }
        }
    }

    dropTargetIndex = -1;
    repaint();
}

void PluginChainView::timerCallback()
{
    // Update CPU usage displays
    for (auto& slot : slotComponents)
    {
        slot->updateDisplay();
    }
}

void PluginChainView::refresh()
{
    updateSlots();
}

void PluginChainView::updateSlots()
{
    auto& chain = application.getVSTChain();

    slotComponents.clear();

    for (int i = 0; i < VSTChain::MAX_PLUGINS; ++i)
    {
        auto slot = std::make_unique<PluginSlotComponent>(application, i);

        slot->onOpenEditor = [this](int index) { openPluginEditor(index); };
        slot->onRemove = [this](int index) { application.getVSTChain().removePlugin(index); };
        slot->onBypassToggle = [this](int index)
        {
            auto& c = application.getVSTChain();
            c.setPluginBypass(index, !c.isPluginBypassed(index));
        };

        addAndMakeVisible(slot.get());
        slotComponents.push_back(std::move(slot));
    }

    // Check for ARC plugin
    arcCompareButton->setVisible(chain.hasARCPlugin());

    resized();
    repaint();
}

int PluginChainView::getSlotIndexAtPosition(const juce::Point<int>& pos) const
{
    for (size_t i = 0; i < slotComponents.size(); ++i)
    {
        if (slotComponents[i]->getBounds().contains(pos))
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void PluginChainView::openPluginEditor(int slotIndex)
{
    auto* plugin = application.getVSTChain().getPlugin(slotIndex);
    if (plugin != nullptr && plugin->hasEditor())
    {
        // The plugin window manager is handled at the main window level
        // This would create a new editor window
        auto* editor = plugin->createEditor();
        if (editor != nullptr)
        {
            auto window = std::make_unique<juce::DocumentWindow>(
                plugin->getName(),
                Colors::background,
                juce::DocumentWindow::closeButton);

            window->setContentOwned(editor, true);
            window->setUsingNativeTitleBar(true);
            window->centreWithSize(editor->getWidth(), editor->getHeight());
            window->setVisible(true);

            // Note: In a full implementation, the window lifecycle should be
            // managed by a PluginWindowManager
        }
    }
}

void PluginChainView::closePluginEditor(int slotIndex)
{
    auto* plugin = application.getVSTChain().getPlugin(slotIndex);
    if (plugin != nullptr)
    {
        plugin->closeEditor();
    }
}

} // namespace ARC

