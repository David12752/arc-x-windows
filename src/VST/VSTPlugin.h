/*
 * ============================================================================
 * ARC System Audio Host - VST Plugin Header
 * Individual VST3 plugin wrapper
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include <memory>
#include <atomic>
#include <functional>

namespace ARC
{

//=============================================================================
/**
 * Plugin state information
 */
struct PluginState
{
    juce::MemoryBlock data;
    juce::String presetName;
    juce::Time savedTime;

    bool isValid() const { return data.getSize() > 0; }
};

//=============================================================================
/**
 * Plugin CPU usage information
 */
struct PluginCPUInfo
{
    double currentUsage{0.0};
    double peakUsage{0.0};
    double averageUsage{0.0};
    int sampleCount{0};
};

//=============================================================================
/**
 * VST Plugin wrapper class
 * Encapsulates a loaded VST3 plugin instance with additional features
 */
class VSTPlugin
{
public:
    //=========================================================================
    using ParameterChangeCallback = std::function<void(int, float)>;
    using StateChangeCallback = std::function<void()>;

    //=========================================================================
    VSTPlugin(std::unique_ptr<juce::AudioPluginInstance> pluginInstance,
              const juce::PluginDescription& description);
    ~VSTPlugin();

    //=========================================================================
    // Identification
    const juce::String& getName() const { return name; }
    const juce::String& getManufacturer() const { return manufacturer; }
    const juce::String& getIdentifier() const { return identifier; }
    const juce::PluginDescription& getDescription() const { return pluginDescription; }
    bool isValid() const { return plugin != nullptr; }

    //=========================================================================
    // Audio processing
    void prepareToPlay(double sampleRate, int blockSize);
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer);
    void releaseResources();

    //=========================================================================
    // Bypass
    void setBypass(bool shouldBypass);
    bool isBypassed() const { return bypassed.load(); }
    void toggleBypass() { setBypass(!bypassed.load()); }

    //=========================================================================
    // Wet/dry mix
    void setMix(float mixAmount); // 0.0 = dry, 1.0 = wet
    float getMix() const { return mix.load(); }

    //=========================================================================
    // Input/Output gain
    void setInputGain(float gainDb);
    float getInputGain() const;
    void setOutputGain(float gainDb);
    float getOutputGain() const;

    //=========================================================================
    // State management
    PluginState getState() const;
    void setState(const PluginState& state);
    void saveStateToFile(const juce::File& file);
    bool loadStateFromFile(const juce::File& file);

    //=========================================================================
    // Preset management
    int getNumPresets() const;
    juce::String getPresetName(int index) const;
    void setPreset(int index);
    int getCurrentPreset() const;

    //=========================================================================
    // Parameters
    int getNumParameters() const;
    float getParameter(int index) const;
    void setParameter(int index, float value);
    juce::String getParameterName(int index) const;
    juce::String getParameterLabel(int index) const;
    juce::String getParameterText(int index) const;
    bool isParameterAutomatable(int index) const;

    // Parameter groups
    int getNumParameterGroups() const;
    juce::String getParameterGroupName(int groupIndex) const;
    std::vector<int> getParametersInGroup(int groupIndex) const;

    //=========================================================================
    // Editor
    bool hasEditor() const;
    juce::AudioProcessorEditor* createEditor();
    void closeEditor();
    bool isEditorOpen() const { return editorOpen.load(); }

    //=========================================================================
    // CPU monitoring
    PluginCPUInfo getCPUInfo() const;
    void resetCPUStats();

    //=========================================================================
    // Audio configuration
    int getNumInputChannels() const;
    int getNumOutputChannels() const;
    double getTailLengthSeconds() const;
    int getLatencySamples() const;

    //=========================================================================
    // Plugin info
    bool acceptsMidi() const;
    bool producesMidi() const;
    bool isInstrument() const;
    bool isEffect() const;

    //=========================================================================
    // Callbacks
    void setParameterChangeCallback(ParameterChangeCallback callback);
    void setStateChangeCallback(StateChangeCallback callback);

    //=========================================================================
    // Direct plugin access (use with caution)
    juce::AudioPluginInstance* getPluginInstance() { return plugin.get(); }
    const juce::AudioPluginInstance* getPluginInstance() const { return plugin.get(); }

private:
    //=========================================================================
    void startCPUMeasurement();
    void endCPUMeasurement();
    void updateCPUStats(double usage);

    //=========================================================================
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    juce::PluginDescription pluginDescription;

    juce::String name;
    juce::String manufacturer;
    juce::String identifier;

    std::atomic<bool> bypassed{false};
    std::atomic<float> mix{1.0f};

    juce::LinearSmoothedValue<float> inputGain;
    juce::LinearSmoothedValue<float> outputGain;
    juce::LinearSmoothedValue<float> mixSmoothed;

    juce::AudioBuffer<float> dryBuffer;

    // PDC: when the plugin has internal latency and a wet/dry mix is active,
    // the dry path must be delayed by `plugin->getLatencySamples()` samples
    // so both signals are time-aligned before blending.
    // The delay line is a simple circular buffer sized at prepareToPlay().
    struct DryDelayLine
    {
        static constexpr int MAX_CH = 2;
        std::array<std::vector<float>, MAX_CH> ring;
        int writeHead{0};
        int delayLength{0};

        void resize(int capacity, int delay)
        {
            delayLength = delay;
            for (auto& ch : ring)
                ch.assign(static_cast<size_t>(capacity), 0.0f);
            writeHead = 0;
        }

        void clear()
        {
            for (auto& ch : ring)
                std::fill(ch.begin(), ch.end(), 0.0f);
            writeHead = 0;
        }

        // Read `numSamples` from `src` through the delay and write results
        // into `dst` (may alias `src`).
        void process(const float* const* src, float* const* dst,
                     int numCh, int numSamples)
        {
            if (delayLength == 0 || ring[0].empty())
            {
                // No latency: pass through
                for (int ch = 0; ch < numCh; ++ch)
                    if (src[ch] != dst[ch])
                        juce::FloatVectorOperations::copy(dst[ch], src[ch], numSamples);
                return;
            }

            const int capacity = static_cast<int>(ring[0].size());
            for (int s = 0; s < numSamples; ++s)
            {
                const int wp = writeHead % capacity;
                const int rp = (writeHead - delayLength + capacity * 16) % capacity;
                for (int ch = 0; ch < juce::jmin(numCh, MAX_CH); ++ch)
                {
                    ring[ch][wp] = src[ch][s];
                    dst[ch][s]   = ring[ch][rp];
                }
                ++writeHead;
            }
        }
    } dryDelayLine;

    // CPU monitoring
    mutable std::mutex cpuMutex;
    PluginCPUInfo cpuInfo;
    double cpuMeasureStart{0.0};
    static constexpr int CPU_SAMPLE_SIZE = 100;
    juce::Array<double> cpuSamples;

    // Editor state
    std::atomic<bool> editorOpen{false};
    juce::AudioProcessorEditor* currentEditor{nullptr};

    // Callbacks
    ParameterChangeCallback parameterChangeCallback;
    StateChangeCallback stateChangeCallback;
    std::mutex callbackMutex;

    double currentSampleRate{44100.0};
    int currentBlockSize{512};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VSTPlugin)
};

} // namespace ARC

