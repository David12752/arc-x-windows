/*
 * ============================================================================
 * ARC System Audio Host - VST Plugin Implementation
 * ============================================================================
 */

#include "VSTPlugin.h"
#include "../Utils/Logger.h"

namespace ARC
{

//=============================================================================
VSTPlugin::VSTPlugin(std::unique_ptr<juce::AudioPluginInstance> pluginInstance,
                     const juce::PluginDescription& description)
    : plugin(std::move(pluginInstance)),
      pluginDescription(description)
{
    if (plugin != nullptr)
    {
        name = plugin->getName();
        manufacturer = pluginDescription.manufacturerName;
        identifier = pluginDescription.createIdentifierString();

        Logger::log("VSTPlugin created: " + name + " by " + manufacturer);
    }
    else
    {
        Logger::log("VSTPlugin created with null plugin instance");
    }
}

VSTPlugin::~VSTPlugin()
{
    closeEditor();
    releaseResources();
    Logger::log("VSTPlugin destroyed: " + name);
}

//=============================================================================
void VSTPlugin::prepareToPlay(double sampleRate, int blockSize)
{
    if (plugin == nullptr)
        return;

    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;

    plugin->prepareToPlay(sampleRate, blockSize);
    plugin->setRateAndBufferSizeDetails(sampleRate, blockSize);

    // Initialize smoothed values
    inputGain.reset(sampleRate, 0.02);
    inputGain.setCurrentAndTargetValue(1.0f);

    outputGain.reset(sampleRate, 0.02);
    outputGain.setCurrentAndTargetValue(1.0f);

    mixSmoothed.reset(sampleRate, 0.02);
    mixSmoothed.setCurrentAndTargetValue(mix.load());

    // Prepare dry buffer for wet/dry mix
    dryBuffer.setSize(plugin->getTotalNumOutputChannels(), blockSize);

    // PDC: size the dry-path delay line to the plugin's own latency.
    // After plugin->prepareToPlay(), the latency is stable and queryable.
    const int pluginLatency = plugin->getLatencySamples();
    if (pluginLatency > 0)
    {
        // Ring capacity: latency + blockSize + 1, rounded up to power of two
        const int capacity = juce::nextPowerOfTwo(pluginLatency + blockSize + 1);
        dryDelayLine.resize(capacity, pluginLatency);
    }
    else
    {
        dryDelayLine.clear();
        dryDelayLine.delayLength = 0;
    }

    cpuSamples.clear();

    Logger::log("VSTPlugin prepared: " + name + " @ " + juce::String(sampleRate) +
                " Hz, buffer: " + juce::String(blockSize));
}

void VSTPlugin::processBlock(juce::AudioBuffer<float>& buffer,
                             juce::MidiBuffer& midiBuffer)
{
    if (plugin == nullptr)
        return;

    const int numSamples = buffer.getNumSamples();

    // Apply input gain.
    // Advance once per sample, apply scalar to all channels in the inner loop.
    if (inputGain.isSmoothing() || std::abs(inputGain.getTargetValue() - 1.0f) > 0.0001f)
    {
        const int numCh = buffer.getNumChannels();
        for (int s = 0; s < numSamples; ++s)
        {
            const float g = inputGain.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer(ch)[s] *= g;
        }
    }

    // Handle bypass
    if (bypassed.load())
    {
        return; // Audio passes through unchanged
    }

    // Store dry signal for mix
    float currentMix = mix.load();
    bool needMix = currentMix < 0.999f;

    if (needMix)
    {
        dryBuffer.makeCopyOf(buffer);
    }

    // Process through plugin
    startCPUMeasurement();
    plugin->processBlock(buffer, midiBuffer);
    endCPUMeasurement();

    // Apply wet/dry mix.
    // Advance smoother once per sample so every channel blends with the
    // same mix ratio at the same instant — preserving stereo coherence.
    // The dry signal is passed through the PDC delay line first so it is
    // time-aligned with the plugin's output (which is delayed by its own
    // internal latency).
    if (needMix)
    {
        mixSmoothed.setTargetValue(currentMix);
        const int numCh = buffer.getNumChannels();

        // Build array of raw pointers for the delay-line API
        std::array<const float*, DryDelayLine::MAX_CH> srcPtrs{};
        std::array<float*, DryDelayLine::MAX_CH>       dstPtrs{};
        for (int ch = 0; ch < juce::jmin(numCh, static_cast<int>(DryDelayLine::MAX_CH)); ++ch)
        {
            srcPtrs[ch] = dryBuffer.getReadPointer(ch);
            dstPtrs[ch] = dryBuffer.getWritePointer(ch);
        }

        // Delay the captured dry signal to align it with the wet output
        dryDelayLine.process(srcPtrs.data(), dstPtrs.data(), numCh, numSamples);

        for (int s = 0; s < numSamples; ++s)
        {
            const float mixVal = mixSmoothed.getNextValue();
            const float dryW   = 1.0f - mixVal;
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* wet      = buffer.getWritePointer(ch);
                const float* dry = dryBuffer.getReadPointer(ch);
                wet[s] = dry[s] * dryW + wet[s] * mixVal;
            }
        }
    }

    // Apply output gain (same sample-outer / channel-inner pattern).
    if (outputGain.isSmoothing() || std::abs(outputGain.getTargetValue() - 1.0f) > 0.0001f)
    {
        const int numCh = buffer.getNumChannels();
        for (int s = 0; s < numSamples; ++s)
        {
            const float g = outputGain.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer(ch)[s] *= g;
        }
    }
}

void VSTPlugin::releaseResources()
{
    if (plugin != nullptr)
    {
        plugin->releaseResources();
    }
}

//=============================================================================
void VSTPlugin::setBypass(bool shouldBypass)
{
    bypassed.store(shouldBypass);
    Logger::log(name + " bypass: " + (shouldBypass ? "ON" : "OFF"));
}

void VSTPlugin::setMix(float mixAmount)
{
    mix.store(juce::jlimit(0.0f, 1.0f, mixAmount));
}

//=============================================================================
void VSTPlugin::setInputGain(float gainDb)
{
    inputGain.setTargetValue(juce::Decibels::decibelsToGain(gainDb));
}

float VSTPlugin::getInputGain() const
{
    return juce::Decibels::gainToDecibels(inputGain.getTargetValue());
}

void VSTPlugin::setOutputGain(float gainDb)
{
    outputGain.setTargetValue(juce::Decibels::decibelsToGain(gainDb));
}

float VSTPlugin::getOutputGain() const
{
    return juce::Decibels::gainToDecibels(outputGain.getTargetValue());
}

//=============================================================================
PluginState VSTPlugin::getState() const
{
    PluginState state;

    if (plugin != nullptr)
    {
        plugin->getStateInformation(state.data);
        state.presetName = plugin->getName();
        state.savedTime = juce::Time::getCurrentTime();
    }

    return state;
}

void VSTPlugin::setState(const PluginState& state)
{
    if (plugin != nullptr && state.isValid())
    {
        plugin->setStateInformation(state.data.getData(),
                                   static_cast<int>(state.data.getSize()));
        Logger::log(name + " state loaded");

        std::lock_guard<std::mutex> lock(callbackMutex);
        if (stateChangeCallback)
        {
            stateChangeCallback();
        }
    }
}

void VSTPlugin::saveStateToFile(const juce::File& file)
{
    auto state = getState();

    if (state.isValid())
    {
        file.replaceWithData(state.data.getData(), state.data.getSize());
        Logger::log(name + " state saved to: " + file.getFullPathName());
    }
}

bool VSTPlugin::loadStateFromFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    juce::MemoryBlock data;
    if (file.loadFileAsData(data))
    {
        PluginState state;
        state.data = data;
        state.presetName = file.getFileNameWithoutExtension();
        state.savedTime = file.getLastModificationTime();

        setState(state);
        return true;
    }

    return false;
}

//=============================================================================
int VSTPlugin::getNumPresets() const
{
    return plugin ? plugin->getNumPrograms() : 0;
}

juce::String VSTPlugin::getPresetName(int index) const
{
    return plugin ? plugin->getProgramName(index) : juce::String();
}

void VSTPlugin::setPreset(int index)
{
    if (plugin && index >= 0 && index < plugin->getNumPrograms())
    {
        plugin->setCurrentProgram(index);
        Logger::log(name + " preset changed to: " + getPresetName(index));
    }
}

int VSTPlugin::getCurrentPreset() const
{
    return plugin ? plugin->getCurrentProgram() : 0;
}

//=============================================================================
int VSTPlugin::getNumParameters() const
{
    return plugin ? plugin->getParameters().size() : 0;
}

float VSTPlugin::getParameter(int index) const
{
    if (plugin && index >= 0 && index < plugin->getParameters().size())
    {
        return plugin->getParameters()[index]->getValue();
    }
    return 0.0f;
}

void VSTPlugin::setParameter(int index, float value)
{
    if (plugin && index >= 0 && index < plugin->getParameters().size())
    {
        plugin->getParameters()[index]->setValue(value);

        std::lock_guard<std::mutex> lock(callbackMutex);
        if (parameterChangeCallback)
        {
            parameterChangeCallback(index, value);
        }
    }
}

juce::String VSTPlugin::getParameterName(int index) const
{
    if (plugin && index >= 0 && index < plugin->getParameters().size())
    {
        return plugin->getParameters()[index]->getName(128);
    }
    return {};
}

juce::String VSTPlugin::getParameterLabel(int index) const
{
    if (plugin && index >= 0 && index < plugin->getParameters().size())
    {
        return plugin->getParameters()[index]->getLabel();
    }
    return {};
}

juce::String VSTPlugin::getParameterText(int index) const
{
    if (plugin && index >= 0 && index < plugin->getParameters().size())
    {
        return plugin->getParameters()[index]->getCurrentValueAsText();
    }
    return {};
}

bool VSTPlugin::isParameterAutomatable(int index) const
{
    if (plugin && index >= 0 && index < plugin->getParameters().size())
    {
        return plugin->getParameters()[index]->isAutomatable();
    }
    return false;
}

int VSTPlugin::getNumParameterGroups() const
{
    if (plugin)
    {
        return static_cast<int>(plugin->getParameterTree().getSubgroups(false).size());
    }
    return 0;
}

juce::String VSTPlugin::getParameterGroupName(int groupIndex) const
{
    if (plugin)
    {
        auto subgroups = plugin->getParameterTree().getSubgroups(false);
        if (groupIndex >= 0 && groupIndex < static_cast<int>(subgroups.size()))
        {
            return subgroups[static_cast<size_t>(groupIndex)]->getName();
        }
    }
    return {};
}

std::vector<int> VSTPlugin::getParametersInGroup(int groupIndex) const
{
    std::vector<int> indices;
    // Implementation would iterate through parameter tree
    return indices;
}

//=============================================================================
bool VSTPlugin::hasEditor() const
{
    return plugin && plugin->hasEditor();
}

juce::AudioProcessorEditor* VSTPlugin::createEditor()
{
    if (plugin && plugin->hasEditor())
    {
        currentEditor = plugin->createEditor();
        editorOpen.store(currentEditor != nullptr);
        return currentEditor;
    }
    return nullptr;
}

void VSTPlugin::closeEditor()
{
    if (currentEditor != nullptr)
    {
        delete currentEditor;
        currentEditor = nullptr;
        editorOpen.store(false);
    }
}

//=============================================================================
PluginCPUInfo VSTPlugin::getCPUInfo() const
{
    std::lock_guard<std::mutex> lock(cpuMutex);
    return cpuInfo;
}

void VSTPlugin::resetCPUStats()
{
    std::lock_guard<std::mutex> lock(cpuMutex);
    cpuInfo = PluginCPUInfo();
    cpuSamples.clear();
}

void VSTPlugin::startCPUMeasurement()
{
    cpuMeasureStart = juce::Time::getHighResolutionTicks();
}

void VSTPlugin::endCPUMeasurement()
{
    auto endTime = juce::Time::getHighResolutionTicks();
    double duration = juce::Time::highResolutionTicksToSeconds(endTime - cpuMeasureStart);
    double bufferDuration = currentBlockSize / currentSampleRate;
    double usage = (duration / bufferDuration) * 100.0;

    updateCPUStats(usage);
}

void VSTPlugin::updateCPUStats(double usage)
{
    std::lock_guard<std::mutex> lock(cpuMutex);

    cpuInfo.currentUsage = usage;
    cpuInfo.peakUsage = std::max(cpuInfo.peakUsage, usage);
    cpuInfo.sampleCount++;

    cpuSamples.add(usage);
    if (cpuSamples.size() > CPU_SAMPLE_SIZE)
    {
        cpuSamples.remove(0);
    }

    // Calculate average
    double sum = 0.0;
    for (auto sample : cpuSamples)
    {
        sum += sample;
    }
    cpuInfo.averageUsage = cpuSamples.size() > 0 ? sum / cpuSamples.size() : 0.0;
}

//=============================================================================
int VSTPlugin::getNumInputChannels() const
{
    return plugin ? plugin->getTotalNumInputChannels() : 0;
}

int VSTPlugin::getNumOutputChannels() const
{
    return plugin ? plugin->getTotalNumOutputChannels() : 0;
}

double VSTPlugin::getTailLengthSeconds() const
{
    return plugin ? plugin->getTailLengthSeconds() : 0.0;
}

int VSTPlugin::getLatencySamples() const
{
    return plugin ? plugin->getLatencySamples() : 0;
}

//=============================================================================
bool VSTPlugin::acceptsMidi() const
{
    return plugin && plugin->acceptsMidi();
}

bool VSTPlugin::producesMidi() const
{
    return plugin && plugin->producesMidi();
}

bool VSTPlugin::isInstrument() const
{
    return pluginDescription.isInstrument;
}

bool VSTPlugin::isEffect() const
{
    return !isInstrument();
}

//=============================================================================
void VSTPlugin::setParameterChangeCallback(ParameterChangeCallback callback)
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    parameterChangeCallback = callback;
}

void VSTPlugin::setStateChangeCallback(StateChangeCallback callback)
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    stateChangeCallback = callback;
}

} // namespace ARC

