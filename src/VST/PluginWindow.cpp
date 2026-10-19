/*
 * ============================================================================
 * ARC System Audio Host - Plugin Window Implementation
 * ============================================================================
 */

#include "PluginWindow.h"
#include "../Utils/Logger.h"

namespace ARC
{

//=============================================================================
// PluginWindow Implementation
//=============================================================================

PluginWindow::PluginWindow(VSTPlugin& pluginRef, ClosedCallback onClose)
    : juce::DocumentWindow(pluginRef.getName(),
                          juce::Colour(0xFF1E1E2E),
                          juce::DocumentWindow::closeButton |
                          juce::DocumentWindow::minimiseButton),
      plugin(pluginRef),
      closedCallback(onClose)
{
    if (plugin.hasEditor())
    {
        auto* editor = plugin.createEditor();
        if (editor != nullptr)
        {
            setContentOwned(editor, true);
            setResizable(editor->isResizable(), false);
        }
    }
    else
    {
        // Use generic editor
        auto* genericEditor = new GenericPluginEditor(*plugin.getPluginInstance());
        setContentOwned(genericEditor, true);
        setResizable(true, false);
    }

    setUsingNativeTitleBar(true);
    setVisible(true);

    // Center on screen
    centreWithSize(getWidth(), getHeight());

    Logger::log("Plugin window opened: " + plugin.getName());
}

PluginWindow::~PluginWindow()
{
    Logger::log("Plugin window closed: " + plugin.getName());
}

//=============================================================================
void PluginWindow::closeButtonPressed()
{
    if (closedCallback)
    {
        closedCallback(&plugin);
    }
    else
    {
        setVisible(false);
    }
}

void PluginWindow::moved()
{
    DocumentWindow::moved();
    lastBounds = getBounds();
}

void PluginWindow::resized()
{
    DocumentWindow::resized();
    lastBounds = getBounds();
}

void PluginWindow::setLastBounds(const juce::Rectangle<int>& bounds)
{
    lastBounds = bounds;
    setBounds(bounds);
}

void PluginWindow::setScaleFactor(float scale)
{
    scaleFactor = scale;
    // Apply scale to editor if possible
    if (auto* editor = dynamic_cast<juce::AudioProcessorEditor*>(getContentComponent()))
    {
        editor->setScaleFactor(scale);
    }
}

//=============================================================================
// GenericPluginEditor Implementation
//=============================================================================

GenericPluginEditor::GenericPluginEditor(juce::AudioProcessor& processor)
    : AudioProcessorEditor(processor)
{
    // Create parameter sliders
    for (auto* param : processor.getParameters())
    {
        if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*>(param))
        {
            auto slider = std::make_unique<ParameterSlider>(*rangedParam);
            parameterContainer.addAndMakeVisible(slider.get());
            parameterSliders.push_back(std::move(slider));
        }
    }

    viewport.setViewedComponent(&parameterContainer, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    int height = juce::jmin(600, static_cast<int>(parameterSliders.size()) *
                            (PARAM_HEIGHT + PARAM_SPACING) + 20);
    setSize(400, height);
}

GenericPluginEditor::~GenericPluginEditor() = default;

void GenericPluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1E1E2E));

    if (parameterSliders.empty())
    {
        g.setColour(juce::Colours::white);
        g.setFont(16.0f);
        g.drawText("No parameters available",
                   getLocalBounds(),
                   juce::Justification::centred);
    }
}

void GenericPluginEditor::resized()
{
    viewport.setBounds(getLocalBounds().reduced(10));

    int y = 10;
    for (auto& slider : parameterSliders)
    {
        slider->setBounds(0, y, viewport.getWidth() - 20, PARAM_HEIGHT);
        y += PARAM_HEIGHT + PARAM_SPACING;
    }

    parameterContainer.setSize(viewport.getWidth() - 20, y);
}

//=============================================================================
// ParameterSlider Implementation
//=============================================================================

GenericPluginEditor::ParameterSlider::ParameterSlider(juce::RangedAudioParameter& param)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    slider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xFF2E2E3E));
    slider.setColour(juce::Slider::trackColourId, juce::Colour(0xFF00D4AA));
    slider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    addAndMakeVisible(slider);

    label.setText(param.getName(128), juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    label.setFont(juce::Font(14.0f));
    addAndMakeVisible(label);

    attachment = std::make_unique<juce::SliderParameterAttachment>(param, slider, nullptr);
}

void GenericPluginEditor::ParameterSlider::resized()
{
    auto bounds = getLocalBounds();
    label.setBounds(bounds.removeFromTop(20));
    slider.setBounds(bounds);
}

//=============================================================================
// PluginWindowManager Implementation
//=============================================================================

PluginWindowManager::PluginWindowManager() = default;

PluginWindowManager::~PluginWindowManager()
{
    closeAllWindows();
}

PluginWindow* PluginWindowManager::openWindow(VSTPlugin& plugin)
{
    std::lock_guard<std::mutex> lock(windowMutex);

    // Check if already open
    for (auto& window : windows)
    {
        if (&window->getPlugin() == &plugin)
        {
            window->toFront(true);
            return window.get();
        }
    }

    // Create new window
    auto window = std::make_unique<PluginWindow>(plugin, [this](VSTPlugin* p)
    {
        windowClosed(p);
    });

    if (alwaysOnTop)
    {
        window->setAlwaysOnTop(true);
    }

    if (std::abs(globalScale - 1.0f) > 0.01f)
    {
        window->setScaleFactor(globalScale);
    }

    auto* rawPtr = window.get();
    windows.push_back(std::move(window));

    Logger::log("Plugin window manager: opened window for " + plugin.getName());

    return rawPtr;
}

void PluginWindowManager::closeWindow(VSTPlugin& plugin)
{
    std::lock_guard<std::mutex> lock(windowMutex);

    for (auto it = windows.begin(); it != windows.end(); ++it)
    {
        if (&(*it)->getPlugin() == &plugin)
        {
            (*it)->setVisible(false);
            windows.erase(it);
            Logger::log("Plugin window manager: closed window for " + plugin.getName());
            return;
        }
    }
}

void PluginWindowManager::closeAllWindows()
{
    std::lock_guard<std::mutex> lock(windowMutex);

    for (auto& window : windows)
    {
        window->setVisible(false);
    }
    windows.clear();

    Logger::log("Plugin window manager: all windows closed");
}

bool PluginWindowManager::isWindowOpen(const VSTPlugin& plugin) const
{
    std::lock_guard<std::mutex> lock(windowMutex);

    for (const auto& window : windows)
    {
        if (&window->getPlugin() == &plugin)
        {
            return window->isVisible();
        }
    }
    return false;
}

PluginWindow* PluginWindowManager::getWindow(const VSTPlugin& plugin)
{
    std::lock_guard<std::mutex> lock(windowMutex);

    for (auto& window : windows)
    {
        if (&window->getPlugin() == &plugin)
        {
            return window.get();
        }
    }
    return nullptr;
}

int PluginWindowManager::getOpenWindowCount() const
{
    std::lock_guard<std::mutex> lock(windowMutex);
    return static_cast<int>(windows.size());
}

std::vector<PluginWindow*> PluginWindowManager::getAllWindows()
{
    std::lock_guard<std::mutex> lock(windowMutex);

    std::vector<PluginWindow*> result;
    for (auto& window : windows)
    {
        result.push_back(window.get());
    }
    return result;
}

void PluginWindowManager::saveWindowPositions(juce::PropertiesFile& props)
{
    std::lock_guard<std::mutex> lock(windowMutex);

    int index = 0;
    for (const auto& window : windows)
    {
        auto bounds = window->getBounds();
        props.setValue("window_" + juce::String(index) + "_x", bounds.getX());
        props.setValue("window_" + juce::String(index) + "_y", bounds.getY());
        props.setValue("window_" + juce::String(index) + "_w", bounds.getWidth());
        props.setValue("window_" + juce::String(index) + "_h", bounds.getHeight());
        props.setValue("window_" + juce::String(index) + "_plugin",
                       window->getPlugin().getIdentifier());
        ++index;
    }
    props.setValue("window_count", index);
}

void PluginWindowManager::loadWindowPositions(juce::PropertiesFile& props)
{
    // Window positions will be restored when windows are opened
}

void PluginWindowManager::setAlwaysOnTop(bool onTop)
{
    alwaysOnTop = onTop;

    std::lock_guard<std::mutex> lock(windowMutex);

    for (auto& window : windows)
    {
        window->setAlwaysOnTop(onTop);
    }
}

void PluginWindowManager::setGlobalScaleFactor(float scale)
{
    globalScale = scale;

    std::lock_guard<std::mutex> lock(windowMutex);

    for (auto& window : windows)
    {
        window->setScaleFactor(scale);
    }
}

void PluginWindowManager::windowClosed(VSTPlugin* plugin)
{
    juce::MessageManager::callAsync([this, plugin]()
    {
        closeWindow(*plugin);
    });
}

} // namespace ARC

