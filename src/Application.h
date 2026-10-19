/*
 * ============================================================================
 * ARC System Audio Host - Application Header
 * Main application class managing the entire application lifecycle
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include "AudioEngine/AudioDeviceManager.h"
#include "AudioEngine/PluginHost.h"
#include "VST/VSTChain.h"
#include "VST/VSTScanner.h"
#include "Utils/ConfigManager.h"
#include "Utils/PresetManager.h"
#include "UI/MainWindow.h"
#include "UI/LookAndFeel.h"

namespace ARC
{

//=============================================================================
/**
 * Main Application class for ARC System Audio Host
 * Inherits from JUCEApplication to handle application lifecycle
 */
class Application : public juce::JUCEApplication
{
public:
    //=========================================================================
    Application();
    ~Application() override;

    //=========================================================================
    // JUCEApplication interface
    const juce::String getApplicationName() override;
    const juce::String getApplicationVersion() override;
    bool moreThanOneInstanceAllowed() override;

    void initialise(const juce::String& commandLine) override;
    void shutdown() override;
    void systemRequestedQuit() override;
    void anotherInstanceStarted(const juce::String& commandLine) override;

    //=========================================================================
    // Singleton access
    static Application& getInstance();

    //=========================================================================
    // Component accessors
    AudioDeviceManager& getAudioDeviceManager();
    PluginHost& getPluginHost();
    VSTChain& getVSTChain();
    VSTScanner& getVSTScanner();
    ConfigManager& getConfigManager();
    PresetManager& getPresetManager();

    //=========================================================================
    // System tray
    void showSystemTrayMenu();
    void toggleBypass();
    bool isBypassed() const;

    //=========================================================================
    // Global shortcuts
    void registerGlobalShortcuts();
    void unregisterGlobalShortcuts();

private:
    //=========================================================================
    bool initializeAudioSystem();
    bool initializePluginSystem();
    void loadConfiguration();
    void saveConfiguration();
    void createSystemTray();

    //=========================================================================
    std::unique_ptr<ARCLookAndFeel> lookAndFeel;
    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<AudioDeviceManager> audioDeviceManager;
    std::unique_ptr<PluginHost> pluginHost;
    std::unique_ptr<VSTChain> vstChain;
    std::unique_ptr<VSTScanner> vstScanner;
    std::unique_ptr<ConfigManager> configManager;
    std::unique_ptr<PresetManager> presetManager;

    std::unique_ptr<juce::SystemTrayIconComponent> systemTray;

    std::atomic<bool> bypassed{false};
    bool initialized{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Application)
};

//=============================================================================
/**
 * System tray component for quick access
 */
class SystemTrayComponent : public juce::SystemTrayIconComponent
{
public:
    SystemTrayComponent(Application& app);

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    Application& application;
};

} // namespace ARC

