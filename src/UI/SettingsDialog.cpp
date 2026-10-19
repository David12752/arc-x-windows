/*
 * ============================================================================
 * ARC System Audio Host - Settings Dialog Implementation
 * ============================================================================
 */

#include "SettingsDialog.h"
#include "../Application.h"
#include "LookAndFeel.h"

namespace ARC
{

//=============================================================================
// SettingsDialog Implementation
//=============================================================================

SettingsDialog::SettingsDialog(Application& app)
    : application(app)
{
    // Create tabbed component
    tabs = std::make_unique<juce::TabbedComponent>(juce::TabbedButtonBar::TabsAtTop);
    tabs->setColour(juce::TabbedComponent::backgroundColourId, Colors::background);
    tabs->setTabBarDepth(30);

    tabs->addTab("Audio", Colors::backgroundPanel, new AudioSettingsPanel(app), true);
    tabs->addTab("Plugins", Colors::backgroundPanel, new PluginSettingsPanel(app), true);
    tabs->addTab("General", Colors::backgroundPanel, new GeneralSettingsPanel(app), true);

    addAndMakeVisible(tabs.get());

    // Buttons
    okButton = std::make_unique<juce::TextButton>("OK");
    okButton->addListener(this);
    addAndMakeVisible(okButton.get());

    cancelButton = std::make_unique<juce::TextButton>("Cancel");
    cancelButton->addListener(this);
    addAndMakeVisible(cancelButton.get());

    applyButton = std::make_unique<juce::TextButton>("Apply");
    applyButton->addListener(this);
    addAndMakeVisible(applyButton.get());

    setSize(600, 500);
    loadSettings();
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::paint(juce::Graphics& g)
{
    g.fillAll(Colors::background);
}

void SettingsDialog::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    auto buttonRow = bounds.removeFromBottom(35);
    bounds.removeFromBottom(10);

    tabs->setBounds(bounds);

    int buttonWidth = 80;
    int buttonSpacing = 10;

    okButton->setBounds(buttonRow.removeFromRight(buttonWidth));
    buttonRow.removeFromRight(buttonSpacing);
    cancelButton->setBounds(buttonRow.removeFromRight(buttonWidth));
    buttonRow.removeFromRight(buttonSpacing);
    applyButton->setBounds(buttonRow.removeFromRight(buttonWidth));
}

void SettingsDialog::buttonClicked(juce::Button* button)
{
    if (button == okButton.get())
    {
        saveSettings();
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        {
            dw->exitModalState(1);
        }
    }
    else if (button == cancelButton.get())
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        {
            dw->exitModalState(0);
        }
    }
    else if (button == applyButton.get())
    {
        saveSettings();
    }
}

void SettingsDialog::comboBoxChanged(juce::ComboBox* comboBox)
{
    juce::ignoreUnused(comboBox);
}

void SettingsDialog::loadSettings()
{
    // Settings are loaded by each panel
}

void SettingsDialog::saveSettings()
{
    // Save settings from each panel
    if (auto* generalPanel = dynamic_cast<GeneralSettingsPanel*>(tabs->getTabContentComponent(2)))
    {
        generalPanel->saveSettings();
    }
}

void SettingsDialog::scanPlugins()
{
    application.getVSTScanner().startBackgroundScan();
}

void SettingsDialog::addPluginPath()
{
    juce::FileChooser chooser("Select Plugin Folder",
                              juce::File::getSpecialLocation(juce::File::userHomeDirectory));

    if (chooser.browseForDirectory())
    {
        pluginPaths.add(chooser.getResult().getFullPathName());
    }
}

void SettingsDialog::removePluginPath()
{
    // Implementation
}

//=============================================================================
// AudioSettingsPanel Implementation
//=============================================================================

AudioSettingsPanel::AudioSettingsPanel(Application& app)
    : application(app)
{
    // Driver selection
    driverLabel = std::make_unique<juce::Label>("", "Audio Driver:");
    driverLabel->setColour(juce::Label::textColourId, Colors::textSecondary);
    addAndMakeVisible(driverLabel.get());

    driverCombo = std::make_unique<juce::ComboBox>();
    driverCombo->addItem("WASAPI", 1);
    driverCombo->addItem("ASIO", 2);
    driverCombo->setSelectedId(1);
    driverCombo->addListener(this);
    addAndMakeVisible(driverCombo.get());

    // Input device
    inputLabel = std::make_unique<juce::Label>("", "Input Device:");
    inputLabel->setColour(juce::Label::textColourId, Colors::textSecondary);
    addAndMakeVisible(inputLabel.get());

    inputCombo = std::make_unique<juce::ComboBox>();
    inputCombo->addListener(this);
    addAndMakeVisible(inputCombo.get());

    // Output device
    outputLabel = std::make_unique<juce::Label>("", "Output Device:");
    outputLabel->setColour(juce::Label::textColourId, Colors::textSecondary);
    addAndMakeVisible(outputLabel.get());

    outputCombo = std::make_unique<juce::ComboBox>();
    outputCombo->addListener(this);
    addAndMakeVisible(outputCombo.get());

    // Sample rate
    sampleRateLabel = std::make_unique<juce::Label>("", "Sample Rate:");
    sampleRateLabel->setColour(juce::Label::textColourId, Colors::textSecondary);
    addAndMakeVisible(sampleRateLabel.get());

    sampleRateCombo = std::make_unique<juce::ComboBox>();
    sampleRateCombo->addItem("44100 Hz", 44100);
    sampleRateCombo->addItem("48000 Hz", 48000);
    sampleRateCombo->addItem("88200 Hz", 88200);
    sampleRateCombo->addItem("96000 Hz", 96000);
    sampleRateCombo->addListener(this);
    addAndMakeVisible(sampleRateCombo.get());

    // Buffer size
    bufferLabel = std::make_unique<juce::Label>("", "Buffer Size:");
    bufferLabel->setColour(juce::Label::textColourId, Colors::textSecondary);
    addAndMakeVisible(bufferLabel.get());

    bufferCombo = std::make_unique<juce::ComboBox>();
    bufferCombo->addItem("64 samples", 64);
    bufferCombo->addItem("128 samples", 128);
    bufferCombo->addItem("256 samples", 256);
    bufferCombo->addItem("512 samples", 512);
    bufferCombo->addItem("1024 samples", 1024);
    bufferCombo->addListener(this);
    addAndMakeVisible(bufferCombo.get());

    // Latency display
    latencyLabel = std::make_unique<juce::Label>("", "Latency: 0 ms");
    latencyLabel->setColour(juce::Label::textColourId, Colors::textDim);
    addAndMakeVisible(latencyLabel.get());

    refreshDevices();
}

AudioSettingsPanel::~AudioSettingsPanel() = default;

void AudioSettingsPanel::paint(juce::Graphics& g)
{
    g.fillAll(Colors::backgroundPanel);
}

void AudioSettingsPanel::resized()
{
    auto bounds = getLocalBounds().reduced(20);
    int rowHeight = 35;
    int labelWidth = 120;

    auto row = bounds.removeFromTop(rowHeight);
    driverLabel->setBounds(row.removeFromLeft(labelWidth));
    driverCombo->setBounds(row.removeFromLeft(200));

    bounds.removeFromTop(10);
    row = bounds.removeFromTop(rowHeight);
    inputLabel->setBounds(row.removeFromLeft(labelWidth));
    inputCombo->setBounds(row.removeFromLeft(300));

    bounds.removeFromTop(10);
    row = bounds.removeFromTop(rowHeight);
    outputLabel->setBounds(row.removeFromLeft(labelWidth));
    outputCombo->setBounds(row.removeFromLeft(300));

    bounds.removeFromTop(20);
    row = bounds.removeFromTop(rowHeight);
    sampleRateLabel->setBounds(row.removeFromLeft(labelWidth));
    sampleRateCombo->setBounds(row.removeFromLeft(150));

    bounds.removeFromTop(10);
    row = bounds.removeFromTop(rowHeight);
    bufferLabel->setBounds(row.removeFromLeft(labelWidth));
    bufferCombo->setBounds(row.removeFromLeft(150));

    bounds.removeFromTop(20);
    latencyLabel->setBounds(bounds.removeFromTop(25));
}

void AudioSettingsPanel::comboBoxChanged(juce::ComboBox* comboBox)
{
    auto& deviceManager = application.getAudioDeviceManager();

    if (comboBox == inputCombo.get())
    {
        deviceManager.setInputDevice(inputCombo->getText());
    }
    else if (comboBox == outputCombo.get())
    {
        deviceManager.setOutputDevice(outputCombo->getText());
    }
    else if (comboBox == sampleRateCombo.get())
    {
        deviceManager.setSampleRate(sampleRateCombo->getSelectedId());
    }
    else if (comboBox == bufferCombo.get())
    {
        deviceManager.setBufferSize(bufferCombo->getSelectedId());
    }

    // Update latency display
    auto stats = deviceManager.getAudioStats();
    latencyLabel->setText("Latency: " + juce::String(stats.totalLatencyMs, 1) + " ms",
                          juce::dontSendNotification);
}

void AudioSettingsPanel::refreshDevices()
{
    auto& deviceManager = application.getAudioDeviceManager();

    // Populate input devices
    inputCombo->clear();
    auto inputDevices = deviceManager.getAvailableInputDevices();
    int id = 1;
    for (const auto& device : inputDevices)
    {
        inputCombo->addItem(device.name, id++);
    }
    inputCombo->setText(deviceManager.getCurrentInputDevice(), juce::dontSendNotification);

    // Populate output devices
    outputCombo->clear();
    auto outputDevices = deviceManager.getAvailableOutputDevices();
    id = 1;
    for (const auto& device : outputDevices)
    {
        outputCombo->addItem(device.name, id++);
    }
    outputCombo->setText(deviceManager.getCurrentOutputDevice(), juce::dontSendNotification);

    // Set current values
    sampleRateCombo->setSelectedId(static_cast<int>(deviceManager.getCurrentSampleRate()),
                                    juce::dontSendNotification);
    bufferCombo->setSelectedId(deviceManager.getCurrentBufferSize(), juce::dontSendNotification);

    // Update latency
    auto stats = deviceManager.getAudioStats();
    latencyLabel->setText("Latency: " + juce::String(stats.totalLatencyMs, 1) + " ms",
                          juce::dontSendNotification);
}

//=============================================================================
// PluginSettingsPanel Implementation
//=============================================================================

class PluginSettingsPanel::PathsListModel : public juce::ListBoxModel
{
public:
    PathsListModel(juce::StringArray& p) : paths(p) {}

    int getNumRows() override { return paths.size(); }

    void paintListBoxItem(int rowNumber, juce::Graphics& g,
                          int width, int height, bool rowIsSelected) override
    {
        if (rowIsSelected)
            g.fillAll(Colors::accent.withAlpha(0.2f));

        g.setColour(Colors::textPrimary);
        g.setFont(13.0f);
        g.drawText(paths[rowNumber], 8, 0, width - 16, height,
                   juce::Justification::centredLeft);
    }

private:
    juce::StringArray& paths;
};

PluginSettingsPanel::PluginSettingsPanel(Application& app)
    : application(app)
{
    pathsLabel = std::make_unique<juce::Label>("", "VST3 Search Paths:");
    pathsLabel->setColour(juce::Label::textColourId, Colors::textSecondary);
    addAndMakeVisible(pathsLabel.get());

    pathsModel = std::make_unique<PathsListModel>(paths);

    pathsList = std::make_unique<juce::ListBox>("Paths", pathsModel.get());
    pathsList->setRowHeight(24);
    pathsList->setColour(juce::ListBox::backgroundColourId, Colors::backgroundLight);
    addAndMakeVisible(pathsList.get());

    addButton = std::make_unique<juce::TextButton>("Add...");
    addButton->addListener(this);
    addAndMakeVisible(addButton.get());

    removeButton = std::make_unique<juce::TextButton>("Remove");
    removeButton->addListener(this);
    addAndMakeVisible(removeButton.get());

    scanButton = std::make_unique<juce::TextButton>("Scan Plugins");
    scanButton->addListener(this);
    addAndMakeVisible(scanButton.get());

    // Initialize paths
    auto searchPaths = application.getVSTScanner().getSearchPaths();
    for (const auto& path : searchPaths)
    {
        paths.add(path.getFullPathName());
    }
    pathsList->updateContent();
}

PluginSettingsPanel::~PluginSettingsPanel() = default;

void PluginSettingsPanel::paint(juce::Graphics& g)
{
    g.fillAll(Colors::backgroundPanel);
}

void PluginSettingsPanel::resized()
{
    auto bounds = getLocalBounds().reduced(20);

    pathsLabel->setBounds(bounds.removeFromTop(25));
    bounds.removeFromTop(5);

    auto buttonRow = bounds.removeFromBottom(35);
    bounds.removeFromBottom(10);

    pathsList->setBounds(bounds.removeFromTop(150));

    auto listButtons = bounds.removeFromTop(35);
    addButton->setBounds(listButtons.removeFromLeft(80));
    listButtons.removeFromLeft(10);
    removeButton->setBounds(listButtons.removeFromLeft(80));

    scanButton->setBounds(buttonRow.removeFromLeft(120));
}

void PluginSettingsPanel::buttonClicked(juce::Button* button)
{
    if (button == addButton.get())
    {
        juce::FileChooser chooser("Select Plugin Folder",
                                  juce::File("C:\\Program Files\\Common Files\\VST3"));

        if (chooser.browseForDirectory())
        {
            paths.add(chooser.getResult().getFullPathName());
            application.getVSTScanner().addSearchPath(chooser.getResult());
            pathsList->updateContent();
        }
    }
    else if (button == removeButton.get())
    {
        int selected = pathsList->getSelectedRow();
        if (selected >= 0 && selected < paths.size())
        {
            application.getVSTScanner().removeSearchPath(juce::File(paths[selected]));
            paths.remove(selected);
            pathsList->updateContent();
        }
    }
    else if (button == scanButton.get())
    {
        application.getVSTScanner().startBackgroundScan();
    }
}

//=============================================================================
// GeneralSettingsPanel Implementation
//=============================================================================

GeneralSettingsPanel::GeneralSettingsPanel(Application& app)
    : application(app)
{
    autoStart = std::make_unique<juce::ToggleButton>("Start with Windows");
    addAndMakeVisible(autoStart.get());

    minimizeToTray = std::make_unique<juce::ToggleButton>("Minimize to system tray");
    addAndMakeVisible(minimizeToTray.get());

    loadLastPreset = std::make_unique<juce::ToggleButton>("Load last preset on startup");
    addAndMakeVisible(loadLastPreset.get());

    checkUpdates = std::make_unique<juce::ToggleButton>("Check for updates automatically");
    addAndMakeVisible(checkUpdates.get());

    themeLabel = std::make_unique<juce::Label>("", "Theme:");
    themeLabel->setColour(juce::Label::textColourId, Colors::textSecondary);
    addAndMakeVisible(themeLabel.get());

    themeCombo = std::make_unique<juce::ComboBox>();
    themeCombo->addItem("Dark", 1);
    themeCombo->addItem("Light", 2);
    themeCombo->setSelectedId(1);
    addAndMakeVisible(themeCombo.get());

    accentLabel = std::make_unique<juce::Label>("", "Accent Color:");
    accentLabel->setColour(juce::Label::textColourId, Colors::textSecondary);
    addAndMakeVisible(accentLabel.get());

    accentCombo = std::make_unique<juce::ComboBox>();
    accentCombo->addItem("Teal", 1);
    accentCombo->addItem("Blue", 2);
    accentCombo->addItem("Purple", 3);
    accentCombo->addItem("Orange", 4);
    accentCombo->addItem("Pink", 5);
    accentCombo->setSelectedId(1);
    addAndMakeVisible(accentCombo.get());

    loadSettings();
}

GeneralSettingsPanel::~GeneralSettingsPanel() = default;

void GeneralSettingsPanel::paint(juce::Graphics& g)
{
    g.fillAll(Colors::backgroundPanel);
}

void GeneralSettingsPanel::resized()
{
    auto bounds = getLocalBounds().reduced(20);
    int rowHeight = 30;

    autoStart->setBounds(bounds.removeFromTop(rowHeight));
    minimizeToTray->setBounds(bounds.removeFromTop(rowHeight));
    loadLastPreset->setBounds(bounds.removeFromTop(rowHeight));
    checkUpdates->setBounds(bounds.removeFromTop(rowHeight));

    bounds.removeFromTop(20);

    auto row = bounds.removeFromTop(rowHeight);
    themeLabel->setBounds(row.removeFromLeft(100));
    themeCombo->setBounds(row.removeFromLeft(150));

    bounds.removeFromTop(10);
    row = bounds.removeFromTop(rowHeight);
    accentLabel->setBounds(row.removeFromLeft(100));
    accentCombo->setBounds(row.removeFromLeft(150));
}

void GeneralSettingsPanel::saveSettings()
{
    auto& config = application.getConfigManager();

    config.setValue("autoStart", autoStart->getToggleState());
    config.setValue("minimizeToTray", minimizeToTray->getToggleState());
    config.setValue("loadLastPreset", loadLastPreset->getToggleState());
    config.setValue("checkUpdates", checkUpdates->getToggleState());
    config.setValue("theme", themeCombo->getSelectedId());
    config.setValue("accentColor", accentCombo->getSelectedId());

    config.save();
}

void GeneralSettingsPanel::loadSettings()
{
    auto& config = application.getConfigManager();

    autoStart->setToggleState(config.getValue("autoStart", false), juce::dontSendNotification);
    minimizeToTray->setToggleState(config.getValue("minimizeToTray", true), juce::dontSendNotification);
    loadLastPreset->setToggleState(config.getValue("loadLastPreset", true), juce::dontSendNotification);
    checkUpdates->setToggleState(config.getValue("checkUpdates", true), juce::dontSendNotification);
    themeCombo->setSelectedId(config.getValue("theme", 1), juce::dontSendNotification);
    accentCombo->setSelectedId(config.getValue("accentColor", 1), juce::dontSendNotification);
}

} // namespace ARC

