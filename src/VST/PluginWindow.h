/*
 * ============================================================================
 * ARC System Audio Host - Plugin Window Header
 * Window management for VST3 plugin editors
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include "VSTPlugin.h"
#include <memory>
#include <vector>
#include <functional>

namespace ARC
{

//=============================================================================
/**
 * Plugin editor window
 * Hosts a VST3 plugin's native GUI
 */
class PluginWindow : public juce::DocumentWindow
{
public:
    //=========================================================================
    using ClosedCallback = std::function<void(VSTPlugin*)>;

    //=========================================================================
    PluginWindow(VSTPlugin& plugin, ClosedCallback onClose = nullptr);
    ~PluginWindow() override;

    //=========================================================================
    void closeButtonPressed() override;
    void moved() override;
    void resized() override;

    //=========================================================================
    VSTPlugin& getPlugin() { return plugin; }
    const VSTPlugin& getPlugin() const { return plugin; }

    //=========================================================================
    // Position persistence
    juce::Rectangle<int> getLastBounds() const { return lastBounds; }
    void setLastBounds(const juce::Rectangle<int>& bounds);

    //=========================================================================
    // Scale factor
    void setScaleFactor(float scale);
    float getScaleFactor() const { return scaleFactor; }

private:
    VSTPlugin& plugin;
    ClosedCallback closedCallback;
    juce::Rectangle<int> lastBounds;
    float scaleFactor{1.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginWindow)
};

//=============================================================================
/**
 * Generic plugin editor
 * Shows parameters as sliders when native editor is not available
 */
class GenericPluginEditor : public juce::AudioProcessorEditor
{
public:
    //=========================================================================
    explicit GenericPluginEditor(juce::AudioProcessor& processor);
    ~GenericPluginEditor() override;

    //=========================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    //=========================================================================
    class ParameterSlider : public juce::Component
    {
    public:
        ParameterSlider(juce::RangedAudioParameter& param);
        void resized() override;

    private:
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    //=========================================================================
    juce::Viewport viewport;
    juce::Component parameterContainer;
    std::vector<std::unique_ptr<ParameterSlider>> parameterSliders;

    static constexpr int PARAM_HEIGHT = 60;
    static constexpr int PARAM_SPACING = 4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GenericPluginEditor)
};

//=============================================================================
/**
 * Plugin window manager
 * Manages multiple plugin editor windows
 */
class PluginWindowManager
{
public:
    //=========================================================================
    PluginWindowManager();
    ~PluginWindowManager();

    //=========================================================================
    // Window management
    PluginWindow* openWindow(VSTPlugin& plugin);
    void closeWindow(VSTPlugin& plugin);
    void closeAllWindows();
    bool isWindowOpen(const VSTPlugin& plugin) const;
    PluginWindow* getWindow(const VSTPlugin& plugin);

    //=========================================================================
    // Window enumeration
    int getOpenWindowCount() const;
    std::vector<PluginWindow*> getAllWindows();

    //=========================================================================
    // Persistence
    void saveWindowPositions(juce::PropertiesFile& props);
    void loadWindowPositions(juce::PropertiesFile& props);

    //=========================================================================
    // Global settings
    void setAlwaysOnTop(bool onTop);
    bool isAlwaysOnTop() const { return alwaysOnTop; }

    void setGlobalScaleFactor(float scale);
    float getGlobalScaleFactor() const { return globalScale; }

private:
    //=========================================================================
    void windowClosed(VSTPlugin* plugin);

    //=========================================================================
    std::vector<std::unique_ptr<PluginWindow>> windows;
    mutable std::mutex windowMutex;

    bool alwaysOnTop{false};
    float globalScale{1.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginWindowManager)
};

} // namespace ARC

