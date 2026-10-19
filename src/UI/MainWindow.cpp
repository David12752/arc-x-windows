/*
 * ============================================================================
 * ARC System Audio Host - Main Window Implementation
 * ============================================================================
 */

#include "MainWindow.h"
#include "../Application.h"
#include "LookAndFeel.h"

namespace ARC
{

//=============================================================================
// Toolbar Implementation
//=============================================================================

Toolbar::Toolbar(Application& app)
    : application(app)
{
    // Power button
    powerButton = std::make_unique<juce::TextButton>("POWER");
    powerButton->setClickingTogglesState(true);
    powerButton->setToggleState(true, juce::dontSendNotification);
    powerButton->addListener(this);
    addAndMakeVisible(powerButton.get());

    // Input device
    inputLabel = std::make_unique<juce::Label>("", "Input:");
    inputLabel->setColour(juce::Label::textColourId, Colors::textSecondary);
    addAndMakeVisible(inputLabel.get());

    inputDeviceCombo = std::make_unique<juce::ComboBox>();
    inputDeviceCombo->addListener(this);
    addAndMakeVisible(inputDeviceCombo.get());

    // Output device
    outputLabel = std::make_unique<juce::Label>("", "Output:");
    outputLabel->setColour(juce::Label::textColourId, Colors::textSecondary);
    addAndMakeVisible(outputLabel.get());

    outputDeviceCombo = std::make_unique<juce::ComboBox>();
    outputDeviceCombo->addListener(this);
    addAndMakeVisible(outputDeviceCombo.get());

    // Sample rate
    sampleRateLabel = std::make_unique<juce::Label>("", "Rate:");
    sampleRateLabel->setColour(juce::Label::textColourId, Colors::textSecondary);
    addAndMakeVisible(sampleRateLabel.get());

    sampleRateCombo = std::make_unique<juce::ComboBox>();
    sampleRateCombo->addItem("44100 Hz", 44100);
    sampleRateCombo->addItem("48000 Hz", 48000);
    sampleRateCombo->addItem("88200 Hz", 88200);
    sampleRateCombo->addItem("96000 Hz", 96000);
    sampleRateCombo->setSelectedId(48000, juce::dontSendNotification);
    sampleRateCombo->addListener(this);
    addAndMakeVisible(sampleRateCombo.get());

    // Buffer size
    bufferLabel = std::make_unique<juce::Label>("", "Buffer:");
    bufferLabel->setColour(juce::Label::textColourId, Colors::textSecondary);
    addAndMakeVisible(bufferLabel.get());

    bufferSizeCombo = std::make_unique<juce::ComboBox>();
    bufferSizeCombo->addItem("64", 64);
    bufferSizeCombo->addItem("128", 128);
    bufferSizeCombo->addItem("256", 256);
    bufferSizeCombo->addItem("512", 512);
    bufferSizeCombo->addItem("1024", 1024);
    bufferSizeCombo->setSelectedId(256, juce::dontSendNotification);
    bufferSizeCombo->addListener(this);
    addAndMakeVisible(bufferSizeCombo.get());

    // Settings button
    settingsButton = std::make_unique<juce::TextButton>("Settings");
    settingsButton->addListener(this);
    addAndMakeVisible(settingsButton.get());

    refreshDevices();
}

Toolbar::~Toolbar() = default;

void Toolbar::paint(juce::Graphics& g)
{
    g.setColour(Colors::backgroundPanel);
    g.fillRect(getLocalBounds());

    g.setColour(Colors::border);
    g.drawLine(0, getHeight() - 0.5f, getWidth(), getHeight() - 0.5f);
}

void Toolbar::resized()
{
    auto bounds = getLocalBounds().reduced(10, 8);

    powerButton->setBounds(bounds.removeFromLeft(80));
    bounds.removeFromLeft(20);

    inputLabel->setBounds(bounds.removeFromLeft(40));
    inputDeviceCombo->setBounds(bounds.removeFromLeft(180));
    bounds.removeFromLeft(15);

    outputLabel->setBounds(bounds.removeFromLeft(50));
    outputDeviceCombo->setBounds(bounds.removeFromLeft(180));
    bounds.removeFromLeft(15);

    sampleRateLabel->setBounds(bounds.removeFromLeft(35));
    sampleRateCombo->setBounds(bounds.removeFromLeft(100));
    bounds.removeFromLeft(15);

    bufferLabel->setBounds(bounds.removeFromLeft(45));
    bufferSizeCombo->setBounds(bounds.removeFromLeft(80));

    settingsButton->setBounds(bounds.removeFromRight(80));
}

void Toolbar::buttonClicked(juce::Button* button)
{
    if (button == powerButton.get())
    {
        bool isOn = powerButton->getToggleState();
        application.getVSTChain().setMasterBypass(!isOn);
        updatePowerState(isOn);
    }
    else if (button == settingsButton.get())
    {
        // Show settings dialog
        auto* parent = findParentComponentOfClass<MainWindow>();
        if (parent != nullptr)
        {
            parent->showSettings();
        }
    }
}

void Toolbar::comboBoxChanged(juce::ComboBox* comboBox)
{
    auto& deviceManager = application.getAudioDeviceManager();

    if (comboBox == inputDeviceCombo.get())
    {
        deviceManager.setInputDevice(inputDeviceCombo->getText());
    }
    else if (comboBox == outputDeviceCombo.get())
    {
        deviceManager.setOutputDevice(outputDeviceCombo->getText());
    }
    else if (comboBox == sampleRateCombo.get())
    {
        deviceManager.setSampleRate(sampleRateCombo->getSelectedId());
    }
    else if (comboBox == bufferSizeCombo.get())
    {
        deviceManager.setBufferSize(bufferSizeCombo->getSelectedId());
    }
}

void Toolbar::refreshDevices()
{
    auto& deviceManager = application.getAudioDeviceManager();

    // Populate input devices
    inputDeviceCombo->clear();
    auto inputDevices = deviceManager.getAvailableInputDevices();
    int id = 1;
    for (const auto& device : inputDevices)
    {
        inputDeviceCombo->addItem(device.name, id++);
    }

    // Populate output devices
    outputDeviceCombo->clear();
    auto outputDevices = deviceManager.getAvailableOutputDevices();
    id = 1;
    for (const auto& device : outputDevices)
    {
        outputDeviceCombo->addItem(device.name, id++);
    }

    // Select current devices
    inputDeviceCombo->setText(deviceManager.getCurrentInputDevice(), juce::dontSendNotification);
    outputDeviceCombo->setText(deviceManager.getCurrentOutputDevice(), juce::dontSendNotification);

    // Update sample rate and buffer size
    auto currentRate = static_cast<int>(deviceManager.getCurrentSampleRate());
    sampleRateCombo->setSelectedId(currentRate, juce::dontSendNotification);

    auto currentBuffer = deviceManager.getCurrentBufferSize();
    bufferSizeCombo->setSelectedId(currentBuffer, juce::dontSendNotification);
}

void Toolbar::updatePowerState(bool isOn)
{
    if (isOn)
    {
        powerButton->setColour(juce::TextButton::buttonOnColourId, Colors::accent);
    }
    else
    {
        powerButton->setColour(juce::TextButton::buttonOnColourId, Colors::bypassed);
    }
}

//=============================================================================
// StatusBar Implementation
//=============================================================================

StatusBar::StatusBar(Application& app)
    : application(app)
{
    // VU Meters
    vuMeterL = std::make_unique<VUMeter>();
    vuMeterL->setOrientation(VUMeter::Orientation::Horizontal);
    addAndMakeVisible(vuMeterL.get());

    vuMeterR = std::make_unique<VUMeter>();
    vuMeterR->setOrientation(VUMeter::Orientation::Horizontal);
    addAndMakeVisible(vuMeterR.get());

    // Master volume
    masterVolume = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal,
                                                   juce::Slider::TextBoxRight);
    masterVolume->setRange(-60.0, 12.0, 0.1);
    masterVolume->setValue(0.0);
    masterVolume->setTextValueSuffix(" dB");
    masterVolume->onValueChange = [this]()
    {
        float gain = juce::Decibels::decibelsToGain(static_cast<float>(masterVolume->getValue()));
        application.getAudioDeviceManager().setMasterVolume(gain);
    };
    addAndMakeVisible(masterVolume.get());

    // CPU label
    cpuLabel = std::make_unique<juce::Label>("", "CPU: 0%");
    cpuLabel->setColour(juce::Label::textColourId, Colors::textSecondary);
    addAndMakeVisible(cpuLabel.get());

    // Latency label
    latencyLabel = std::make_unique<juce::Label>("", "Latency: 0 ms");
    latencyLabel->setColour(juce::Label::textColourId, Colors::textSecondary);
    addAndMakeVisible(latencyLabel.get());

    // Bypass button
    bypassButton = std::make_unique<juce::TextButton>("BYPASS");
    bypassButton->setClickingTogglesState(true);
    bypassButton->onClick = [this]()
    {
        application.toggleBypass();
    };
    addAndMakeVisible(bypassButton.get());

    startTimerHz(30);
}

StatusBar::~StatusBar()
{
    stopTimer();
}

void StatusBar::paint(juce::Graphics& g)
{
    g.setColour(Colors::backgroundPanel);
    g.fillRect(getLocalBounds());

    g.setColour(Colors::border);
    g.drawLine(0, 0.5f, getWidth(), 0.5f);

    // Labels
    g.setColour(Colors::textDim);
    g.setFont(12.0f);
    g.drawText("L", 10, 10, 15, 16, juce::Justification::centred);
    g.drawText("R", 10, 32, 15, 16, juce::Justification::centred);
    g.drawText("Master", 280, 10, 50, 40, juce::Justification::centredLeft);
}

void StatusBar::resized()
{
    auto bounds = getLocalBounds().reduced(10, 8);

    // VU Meters on left
    auto meterArea = bounds.removeFromLeft(250).reduced(0, 2);
    meterArea.removeFromLeft(15); // Space for labels
    vuMeterL->setBounds(meterArea.removeFromTop(18));
    meterArea.removeFromTop(4);
    vuMeterR->setBounds(meterArea.removeFromTop(18));

    bounds.removeFromLeft(10);

    // Master volume
    bounds.removeFromLeft(50); // "Master" label space
    masterVolume->setBounds(bounds.removeFromLeft(200));

    // Right side info
    bypassButton->setBounds(bounds.removeFromRight(80));
    bounds.removeFromRight(20);

    latencyLabel->setBounds(bounds.removeFromRight(120));
    cpuLabel->setBounds(bounds.removeFromRight(80));
}

void StatusBar::timerCallback()
{
    auto stats = application.getAudioDeviceManager().getAudioStats();

    // Update VU meters
    vuMeterL->setLevel(stats.peakLevelL);
    vuMeterR->setLevel(stats.peakLevelR);

    // Update CPU
    cpuLabel->setText("CPU: " + juce::String(stats.cpuUsage, 1) + "%",
                      juce::dontSendNotification);

    // Color code CPU
    if (stats.cpuUsage > 80)
        cpuLabel->setColour(juce::Label::textColourId, Colors::error);
    else if (stats.cpuUsage > 50)
        cpuLabel->setColour(juce::Label::textColourId, Colors::warning);
    else
        cpuLabel->setColour(juce::Label::textColourId, Colors::textSecondary);

    // Update latency
    latencyLabel->setText("Latency: " + juce::String(stats.totalLatencyMs, 1) + " ms",
                          juce::dontSendNotification);

    // Update bypass button state
    bypassButton->setToggleState(application.isBypassed(), juce::dontSendNotification);
}

//=============================================================================
// MainContent Implementation
//=============================================================================

MainContent::MainContent(Application& app)
    : application(app)
{
    pluginBrowser = std::make_unique<PluginBrowser>(app);
    addAndMakeVisible(pluginBrowser.get());

    pluginChainView = std::make_unique<PluginChainView>(app);
    addAndMakeVisible(pluginChainView.get());

    spectrumAnalyzer = std::make_unique<SpectrumAnalyzer>();
    addAndMakeVisible(spectrumAnalyzer.get());
}

MainContent::~MainContent() = default;

void MainContent::paint(juce::Graphics& g)
{
    g.fillAll(Colors::background);
}

void MainContent::resized()
{
    auto bounds = getLocalBounds();

    // Left panel - Plugin browser
    pluginBrowser->setBounds(bounds.removeFromLeft(leftPanelWidth));

    // Bottom - Spectrum analyzer
    spectrumAnalyzer->setBounds(bounds.removeFromBottom(120));

    // Center - Plugin chain
    pluginChainView->setBounds(bounds);
}

//=============================================================================
// MainWindow Implementation
//=============================================================================

MainWindow::MainWindow(const juce::String& name, Application& app)
    : DocumentWindow(name,
                    Colors::background,
                    DocumentWindow::allButtons),
      application(app)
{
    setUsingNativeTitleBar(true);
    setResizable(true, true);
    setResizeLimits(1024, 600, 3840, 2160);

    // Create child components
    toolbar = std::make_unique<Toolbar>(app);
    mainContent = std::make_unique<MainContent>(app);
    statusBar = std::make_unique<StatusBar>(app);

    // Create container
    containerComponent = std::make_unique<MainComponent>(*toolbar, *mainContent, *statusBar);
    setContentNonOwned(containerComponent.get(), false);

    // Create plugin window manager
    pluginWindowManager = std::make_unique<PluginWindowManager>();

    // Set initial size
    centreWithSize(1400, 900);
    setVisible(true);
}

MainWindow::~MainWindow() = default;

void MainWindow::closeButtonPressed()
{
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

void MainWindow::showSettings()
{
    juce::DialogWindow::LaunchOptions options;

    auto* settings = new SettingsDialog(application);
    options.content.setOwned(settings);
    options.dialogTitle = "Settings";
    options.dialogBackgroundColour = Colors::background;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;

    options.launchAsync();
}

void MainWindow::refreshUI()
{
    toolbar->refreshDevices();
    mainContent->getPluginBrowser().refresh();
    mainContent->getPluginChainView().refresh();
}

} // namespace ARC

