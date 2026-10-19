/*
 * ============================================================================
 * ARC System Audio Host - Application Implementation
 * ============================================================================
 */

#include "Application.h"
#include "Utils/Logger.h"

namespace ARC
{

//=============================================================================
Application::Application()
{
    Logger::log("ARC System Audio Host starting...");
}

Application::~Application()
{
    Logger::log("ARC System Audio Host shutting down...");
}

//=============================================================================
const juce::String Application::getApplicationName()
{
    return "ARC System Audio Host";
}

const juce::String Application::getApplicationVersion()
{
    return "1.0.0";
}

bool Application::moreThanOneInstanceAllowed()
{
    return false;
}

//=============================================================================
void Application::initialise(const juce::String& commandLine)
{
    juce::ignoreUnused(commandLine);

    Logger::log("Initializing application...");

    // Initialize look and feel
    lookAndFeel = std::make_unique<ARCLookAndFeel>();
    juce::LookAndFeel::setDefaultLookAndFeel(lookAndFeel.get());

    // Initialize configuration
    configManager = std::make_unique<ConfigManager>();
    if (!configManager)
    {
        Logger::error("Failed to create ConfigManager");
        return;
    }
    
    loadConfiguration();

    // Initialize preset manager
    presetManager = std::make_unique<PresetManager>();
    if (!presetManager)
    {
        Logger::error("Failed to create PresetManager");
        return;
    }

    // Initialize audio system
    if (!initializeAudioSystem())
    {
        Logger::error("Failed to initialize audio system");
        // Continue anyway, user might fix device settings later
    }

    // Initialize plugin system
    if (!initializePluginSystem())
    {
        Logger::error("Failed to initialize plugin system");
        // Continue anyway, plugins can be scanned later
    }

    // Create main window
    mainWindow = std::make_unique<MainWindow>("ARC System Audio Host", *this);
    if (!mainWindow)
    {
        Logger::error("Failed to create main window");
        return;
    }

    // Create system tray
    createSystemTray();

    // Register global shortcuts
    registerGlobalShortcuts();

    // Load last preset if configured
    if (configManager && presetManager)
    {
        if (configManager->getValue("loadLastPreset", true))
        {
            auto lastPreset = configManager->getValue("lastPresetPath", juce::String());
            if (lastPreset.isNotEmpty())
            {
                juce::File presetFile(lastPreset);
                if (presetFile.existsAsFile())
                {
                    if (!presetManager->loadPreset(presetFile))
                    {
                        Logger::warning("Failed to load last preset: " + lastPreset);
                    }
                }
                else
                {
                    Logger::warning("Last preset file not found: " + lastPreset);
                }
            }
        }
    }

    initialized = true;
    Logger::log("Application initialized successfully");
}

void Application::shutdown()
{
    Logger::log("Shutting down application...");

    // Unregister shortcuts
    unregisterGlobalShortcuts();

    // Save configuration
    saveConfiguration();

    // Clean up in reverse order
    systemTray.reset();
    mainWindow.reset();
    vstChain.reset();
    vstScanner.reset();
    pluginHost.reset();
    audioDeviceManager.reset();
    presetManager.reset();
    configManager.reset();

    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    lookAndFeel.reset();

    Logger::log("Application shutdown complete");
}

void Application::systemRequestedQuit()
{
    // Check if we should minimize to tray instead
    if (configManager && configManager->getValue("minimizeToTray", false))
    {
        if (mainWindow)
        {
            mainWindow->setVisible(false);
            return;
        }
    }

    quit();
}

void Application::anotherInstanceStarted(const juce::String& commandLine)
{
    juce::ignoreUnused(commandLine);

    // Bring main window to front
    if (mainWindow != nullptr)
    {
        mainWindow->setVisible(true);
        mainWindow->toFront(true);
    }
}

//=============================================================================
Application& Application::getInstance()
{
    auto* app = dynamic_cast<Application*>(JUCEApplication::getInstance());
    jassert(app != nullptr);
    return *app;
}

//=============================================================================
AudioDeviceManager& Application::getAudioDeviceManager()
{
    jassert(audioDeviceManager != nullptr);
    return *audioDeviceManager;
}

PluginHost& Application::getPluginHost()
{
    jassert(pluginHost != nullptr);
    return *pluginHost;
}

VSTChain& Application::getVSTChain()
{
    jassert(vstChain != nullptr);
    return *vstChain;
}

VSTScanner& Application::getVSTScanner()
{
    jassert(vstScanner != nullptr);
    return *vstScanner;
}

ConfigManager& Application::getConfigManager()
{
    jassert(configManager != nullptr);
    return *configManager;
}

PresetManager& Application::getPresetManager()
{
    jassert(presetManager != nullptr);
    return *presetManager;
}

//=============================================================================
void Application::showSystemTrayMenu()
{
    juce::PopupMenu menu;

    menu.addItem(1, "Show Window", true, false);
    menu.addSeparator();
    menu.addItem(2, "Bypass Processing", true, bypassed.load());
    menu.addSeparator();
    menu.addItem(3, "Settings...", true, false);
    menu.addSeparator();
    menu.addItem(4, "Quit", true, false);

    menu.showMenuAsync(juce::PopupMenu::Options(), [this](int result)
    {
        switch (result)
        {
            case 1:
                if (mainWindow)
                {
                    mainWindow->setVisible(true);
                    mainWindow->toFront(true);
                }
                break;
            case 2:
                toggleBypass();
                break;
            case 3:
                // Show settings
                if (mainWindow)
                {
                    mainWindow->showSettings();
                }
                break;
            case 4:
                quit();
                break;
        }
    });
}

void Application::toggleBypass()
{
    bypassed.store(!bypassed.load());
    
    if (vstChain)
    {
        vstChain->setBypassAll(bypassed.load());
    }

    // Update system tray icon
    if (systemTray)
    {
        // Could change icon based on bypass state
    }
}

bool Application::isBypassed() const
{
    return bypassed.load();
}

//=============================================================================
void Application::registerGlobalShortcuts()
{
    // Platform-specific global shortcut registration
    // On Windows, we could use RegisterHotKey
    Logger::log("Registering global shortcuts...");
}

void Application::unregisterGlobalShortcuts()
{
    Logger::log("Unregistering global shortcuts...");
}

//=============================================================================
bool Application::initializeAudioSystem()
{
    Logger::log("Initializing audio system...");

    if (!configManager)
    {
        Logger::error("Cannot initialize audio system: ConfigManager is null");
        return false;
    }

    audioDeviceManager = std::make_unique<AudioDeviceManager>();
    if (!audioDeviceManager)
    {
        Logger::error("Failed to create AudioDeviceManager");
        return false;
    }

    // Get saved device settings
    auto inputDevice = configManager->getValue("inputDevice", juce::String());
    auto outputDevice = configManager->getValue("outputDevice", juce::String());
    auto sampleRate = configManager->getValue("sampleRate", 48000.0);
    auto bufferSize = configManager->getValue("bufferSize", 256);

    // Initialize audio device
    juce::String error = audioDeviceManager->initialize(inputDevice, outputDevice,
                                                         sampleRate, bufferSize);

    if (error.isNotEmpty())
    {
        Logger::warning("Audio initialization warning: " + error);
        // Don't return false for warnings, only for critical errors
    }

    Logger::log("Audio system initialized - Sample Rate: " +
                juce::String(audioDeviceManager->getCurrentSampleRate()) +
                " Hz, Buffer Size: " +
                juce::String(audioDeviceManager->getCurrentBufferSize()));
    
    return true;
}

bool Application::initializePluginSystem()
{
    Logger::log("Initializing plugin system...");

    if (!configManager)
    {
        Logger::error("Cannot initialize plugin system: ConfigManager is null");
        return false;
    }

    // Initialize plugin host
    pluginHost = std::make_unique<PluginHost>();
    if (!pluginHost)
    {
        Logger::error("Failed to create PluginHost");
        return false;
    }

    // Initialize VST scanner with configured paths
    vstScanner = std::make_unique<VSTScanner>();
    if (!vstScanner)
    {
        Logger::error("Failed to create VSTScanner");
        return false;
    }

    // Add default VST3 paths
    vstScanner->addSearchPath(juce::File("C:\\Program Files\\Common Files\\VST3"));
    vstScanner->addSearchPath(juce::File("C:\\Program Files (x86)\\Common Files\\VST3"));

    // Add custom paths from config
    auto customPaths = configManager->getValue("vstSearchPaths", juce::String());
    if (customPaths.isNotEmpty())
    {
        juce::StringArray paths;
        paths.addTokens(customPaths, ";", "\"");
        for (const auto& path : paths)
        {
            if (path.isNotEmpty())
            {
                juce::File pathFile(path);
                if (pathFile.exists() || pathFile.isDirectory())
                {
                    vstScanner->addSearchPath(pathFile);
                }
                else
                {
                    Logger::warning("Plugin search path does not exist: " + path);
                }
            }
        }
    }

    // Create VST chain
    vstChain = std::make_unique<VSTChain>(*pluginHost);
    if (!vstChain)
    {
        Logger::error("Failed to create VSTChain");
        return false;
    }

    // Connect chain to audio device
    if (audioDeviceManager)
    {
        audioDeviceManager->setAudioCallback(vstChain.get());
    }
    else
    {
        Logger::warning("Cannot connect VST chain: AudioDeviceManager is null");
    }

    // Start plugin scan in background
    if (vstScanner)
    {
        vstScanner->startBackgroundScan();
    }

    Logger::log("Plugin system initialized");
    return true;
}

void Application::loadConfiguration()
{
    Logger::log("Loading configuration...");
    configManager->load();
}

void Application::saveConfiguration()
{
    Logger::log("Saving configuration...");

    if (audioDeviceManager)
    {
        configManager->setValue("sampleRate", audioDeviceManager->getCurrentSampleRate());
        configManager->setValue("bufferSize", audioDeviceManager->getCurrentBufferSize());
    }

    configManager->save();
}

void Application::createSystemTray()
{
    Logger::log("Creating system tray...");

    systemTray = std::make_unique<SystemTrayComponent>(*this);

    // Create a simple default icon
    juce::Image iconImage(juce::Image::ARGB, 32, 32, true);
    juce::Graphics g(iconImage);
    g.setColour(juce::Colour(0xFF00D4AA));
    g.fillEllipse(4.0f, 4.0f, 24.0f, 24.0f);
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText("A", 0, 0, 32, 32, juce::Justification::centred);

    systemTray->setIconImage(iconImage, iconImage);
    systemTray->setIconTooltip("ARC System Audio Host");
}

//=============================================================================
// SystemTrayComponent implementation
//=============================================================================

SystemTrayComponent::SystemTrayComponent(Application& app)
    : application(app)
{
}

void SystemTrayComponent::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
    {
        application.showSystemTrayMenu();
    }
}

void SystemTrayComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    
    // Double click shows the main window
    auto& app = Application::getInstance();
    // Access main window through the public interface
}

} // namespace ARC

