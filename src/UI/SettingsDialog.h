/*
 * ============================================================================
 * ARC System Audio Host - Settings Dialog Header
 * Application settings and configuration
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"

namespace ARC
{

class Application;

//=============================================================================
/**
 * Settings dialog component
 */
class SettingsDialog : public juce::Component,
                       public juce::Button::Listener,
                       public juce::ComboBox::Listener
{
public:
    //=========================================================================
    SettingsDialog(Application& app);
    ~SettingsDialog() override;

    //=========================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

    //=========================================================================
    void buttonClicked(juce::Button* button) override;
    void comboBoxChanged(juce::ComboBox* comboBox) override;

private:
    //=========================================================================
    void loadSettings();
    void saveSettings();
    void scanPlugins();
    void addPluginPath();
    void removePluginPath();

    //=========================================================================
    Application& application;

    // Tab buttons
    std::unique_ptr<juce::TabbedComponent> tabs;

    // Audio settings
    std::unique_ptr<juce::ComboBox> audioDriverCombo;
    std::unique_ptr<juce::ComboBox> inputDeviceCombo;
    std::unique_ptr<juce::ComboBox> outputDeviceCombo;
    std::unique_ptr<juce::ComboBox> sampleRateCombo;
    std::unique_ptr<juce::ComboBox> bufferSizeCombo;

    // Plugin settings
    std::unique_ptr<juce::ListBox> pluginPathsList;
    std::unique_ptr<juce::TextButton> addPathButton;
    std::unique_ptr<juce::TextButton> removePathButton;
    std::unique_ptr<juce::TextButton> scanButton;
    std::unique_ptr<juce::ProgressBar> scanProgress;

    // General settings
    std::unique_ptr<juce::ToggleButton> autoStartCheck;
    std::unique_ptr<juce::ToggleButton> minimizeToTrayCheck;
    std::unique_ptr<juce::ToggleButton> loadLastPresetCheck;
    std::unique_ptr<juce::ComboBox> accentColorCombo;

    // Buttons
    std::unique_ptr<juce::TextButton> okButton;
    std::unique_ptr<juce::TextButton> cancelButton;
    std::unique_ptr<juce::TextButton> applyButton;

    // Plugin paths data
    juce::StringArray pluginPaths;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsDialog)
};

//=============================================================================
/**
 * Audio settings panel
 */
class AudioSettingsPanel : public juce::Component,
                           public juce::ComboBox::Listener
{
public:
    AudioSettingsPanel(Application& app);
    ~AudioSettingsPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void comboBoxChanged(juce::ComboBox* comboBox) override;

    void refreshDevices();

private:
    Application& application;

    std::unique_ptr<juce::Label> driverLabel;
    std::unique_ptr<juce::ComboBox> driverCombo;
    std::unique_ptr<juce::Label> inputLabel;
    std::unique_ptr<juce::ComboBox> inputCombo;
    std::unique_ptr<juce::Label> outputLabel;
    std::unique_ptr<juce::ComboBox> outputCombo;
    std::unique_ptr<juce::Label> sampleRateLabel;
    std::unique_ptr<juce::ComboBox> sampleRateCombo;
    std::unique_ptr<juce::Label> bufferLabel;
    std::unique_ptr<juce::ComboBox> bufferCombo;

    std::unique_ptr<juce::Label> latencyLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSettingsPanel)
};

//=============================================================================
/**
 * Plugin settings panel
 */
class PluginSettingsPanel : public juce::Component,
                            public juce::Button::Listener
{
public:
    PluginSettingsPanel(Application& app);
    ~PluginSettingsPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void buttonClicked(juce::Button* button) override;

private:
    Application& application;

    std::unique_ptr<juce::Label> pathsLabel;
    std::unique_ptr<juce::ListBox> pathsList;
    std::unique_ptr<juce::TextButton> addButton;
    std::unique_ptr<juce::TextButton> removeButton;
    std::unique_ptr<juce::TextButton> scanButton;

    std::unique_ptr<juce::Label> blacklistLabel;
    std::unique_ptr<juce::ListBox> blacklistList;
    std::unique_ptr<juce::TextButton> unblacklistButton;

    juce::StringArray paths;
    juce::StringArray blacklist;

    class PathsListModel;
    std::unique_ptr<PathsListModel> pathsModel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginSettingsPanel)
};

//=============================================================================
/**
 * General settings panel
 */
class GeneralSettingsPanel : public juce::Component
{
public:
    GeneralSettingsPanel(Application& app);
    ~GeneralSettingsPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void saveSettings();
    void loadSettings();

private:
    Application& application;

    std::unique_ptr<juce::ToggleButton> autoStart;
    std::unique_ptr<juce::ToggleButton> minimizeToTray;
    std::unique_ptr<juce::ToggleButton> loadLastPreset;
    std::unique_ptr<juce::ToggleButton> checkUpdates;

    std::unique_ptr<juce::Label> themeLabel;
    std::unique_ptr<juce::ComboBox> themeCombo;

    std::unique_ptr<juce::Label> accentLabel;
    std::unique_ptr<juce::ComboBox> accentCombo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GeneralSettingsPanel)
};

} // namespace ARC

