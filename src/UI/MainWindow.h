/*
 * ============================================================================
 * ARC System Audio Host - Main Window Header
 * Primary application window with all UI panels
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include "PluginBrowser.h"
#include "PluginChainView.h"
#include "SettingsDialog.h"
#include "VUMeter.h"
#include "SpectrumAnalyzer.h"
#include "../VST/PluginWindow.h"

namespace ARC
{

// Forward declarations
class Application;

//=============================================================================
/**
 * Toolbar component at the top of the window
 */
class Toolbar : public juce::Component,
                public juce::Button::Listener,
                public juce::ComboBox::Listener
{
public:
    //=========================================================================
    Toolbar(Application& app);
    ~Toolbar() override;

    //=========================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

    //=========================================================================
    void buttonClicked(juce::Button* button) override;
    void comboBoxChanged(juce::ComboBox* comboBox) override;

    //=========================================================================
    void refreshDevices();
    void updatePowerState(bool isOn);

private:
    Application& application;

    std::unique_ptr<juce::TextButton> powerButton;
    std::unique_ptr<juce::ComboBox> inputDeviceCombo;
    std::unique_ptr<juce::ComboBox> outputDeviceCombo;
    std::unique_ptr<juce::ComboBox> sampleRateCombo;
    std::unique_ptr<juce::ComboBox> bufferSizeCombo;
    std::unique_ptr<juce::TextButton> settingsButton;

    std::unique_ptr<juce::Label> inputLabel;
    std::unique_ptr<juce::Label> outputLabel;
    std::unique_ptr<juce::Label> sampleRateLabel;
    std::unique_ptr<juce::Label> bufferLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Toolbar)
};

//=============================================================================
/**
 * Status bar component at the bottom of the window
 */
class StatusBar : public juce::Component,
                  public juce::Timer
{
public:
    //=========================================================================
    StatusBar(Application& app);
    ~StatusBar() override;

    //=========================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

    //=========================================================================
    void timerCallback() override;

private:
    Application& application;

    std::unique_ptr<VUMeter> vuMeterL;
    std::unique_ptr<VUMeter> vuMeterR;
    std::unique_ptr<juce::Slider> masterVolume;
    std::unique_ptr<juce::Label> cpuLabel;
    std::unique_ptr<juce::Label> latencyLabel;
    std::unique_ptr<juce::TextButton> bypassButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatusBar)
};

//=============================================================================
/**
 * Main content component containing all panels
 */
class MainContent : public juce::Component
{
public:
    //=========================================================================
    MainContent(Application& app);
    ~MainContent() override;

    //=========================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

    //=========================================================================
    PluginBrowser& getPluginBrowser() { return *pluginBrowser; }
    PluginChainView& getPluginChainView() { return *pluginChainView; }

private:
    Application& application;

    std::unique_ptr<PluginBrowser> pluginBrowser;
    std::unique_ptr<PluginChainView> pluginChainView;
    std::unique_ptr<SpectrumAnalyzer> spectrumAnalyzer;

    // Resizer bars
    std::unique_ptr<juce::StretchableLayoutManager> layoutManager;
    std::unique_ptr<juce::StretchableLayoutResizerBar> resizerBar;

    int leftPanelWidth{280};
    int rightPanelWidth{300};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContent)
};

//=============================================================================
/**
 * Main application window
 */
class MainWindow : public juce::DocumentWindow
{
public:
    //=========================================================================
    MainWindow(const juce::String& name, Application& app);
    ~MainWindow() override;

    //=========================================================================
    void closeButtonPressed() override;

    //=========================================================================
    void showSettings();
    void refreshUI();

    //=========================================================================
    // Access to child components
    Toolbar& getToolbar() { return *toolbar; }
    MainContent& getMainContent() { return *mainContent; }
    StatusBar& getStatusBar() { return *statusBar; }

private:
    //=========================================================================
    Application& application;

    std::unique_ptr<Toolbar> toolbar;
    std::unique_ptr<MainContent> mainContent;
    std::unique_ptr<StatusBar> statusBar;

    std::unique_ptr<PluginWindowManager> pluginWindowManager;

    // Layout container
    class MainComponent : public juce::Component
    {
    public:
        MainComponent(Toolbar& tb, MainContent& mc, StatusBar& sb)
            : toolbar(tb), mainContent(mc), statusBar(sb)
        {
            addAndMakeVisible(toolbar);
            addAndMakeVisible(mainContent);
            addAndMakeVisible(statusBar);
        }

        void resized() override
        {
            auto bounds = getLocalBounds();
            toolbar.setBounds(bounds.removeFromTop(50));
            statusBar.setBounds(bounds.removeFromBottom(60));
            mainContent.setBounds(bounds);
        }

    private:
        Toolbar& toolbar;
        MainContent& mainContent;
        StatusBar& statusBar;
    };

    std::unique_ptr<MainComponent> containerComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};

} // namespace ARC

