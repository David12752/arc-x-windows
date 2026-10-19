/*
 * ============================================================================
 * ARC System Audio Host - VST Chain Header
 * Manages a chain of VST3 plugins for audio processing
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include "VSTPlugin.h"
#include "../AudioEngine/AudioDeviceManager.h"
#include "../AudioEngine/PDCEngine.h"
#include "../AudioEngine/PluginHost.h"
#include <vector>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <functional>

namespace ARC
{

//=============================================================================
/**
 * Chain slot state
 */
enum class SlotState
{
    Empty,
    Loading,
    Active,
    Bypassed,
    Error
};

//=============================================================================
/**
 * Plugin slot in the chain
 */
struct PluginSlot
{
    int index{0};
    std::unique_ptr<VSTPlugin> plugin;
    SlotState state{SlotState::Empty};
    juce::String errorMessage;
    double cpuUsage{0.0};

    bool isEmpty() const { return state == SlotState::Empty; }
    bool isActive() const { return state == SlotState::Active; }
    bool isBypassed() const { return state == SlotState::Bypassed; }
    bool hasError() const { return state == SlotState::Error; }
};

//=============================================================================
/**
 * Chain state for saving/loading
 */
struct ChainState
{
    struct PluginEntry
    {
        juce::String pluginId;
        juce::MemoryBlock state;
        bool bypassed{false};
        float inputGain{0.0f};
        float outputGain{0.0f};
        float mix{1.0f};
    };

    std::vector<PluginEntry> plugins;
    juce::String presetName;
    juce::Time savedTime;
};

//=============================================================================
/**
 * VST Chain class
 * Manages a chain of VST plugins with processing and routing
 */
class VSTChain : public AudioCallback
{
public:
    //=========================================================================
    static constexpr int MAX_PLUGINS = 10;

    using ChainChangeCallback = std::function<void()>;
    using PluginLoadCallback = std::function<void(int, bool, const juce::String&)>;

    //=========================================================================
    explicit VSTChain(PluginHost& host);
    ~VSTChain() override;

    //=========================================================================
    // AudioCallback interface
    void processBlock(juce::AudioBuffer<float>& buffer,
                     juce::MidiBuffer& midiBuffer) override;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    //=========================================================================
    // Plugin management
    bool addPlugin(const juce::PluginDescription& description, int insertIndex = -1);
    bool addPluginAsync(const juce::PluginDescription& description, int insertIndex = -1);
    bool removePlugin(int index);
    void removeAllPlugins();

    //=========================================================================
    // Plugin access
    VSTPlugin* getPlugin(int index);
    const VSTPlugin* getPlugin(int index) const;
    int getPluginCount() const;
    int getActivePluginCount() const;

    //=========================================================================
    // Slot access
    PluginSlot& getSlot(int index);
    const PluginSlot& getSlot(int index) const;
    std::vector<PluginSlot>& getSlots() { return slots; }

    //=========================================================================
    // Ordering
    bool movePlugin(int fromIndex, int toIndex);
    bool swapPlugins(int indexA, int indexB);

    //=========================================================================
    // Bypass control
    void setPluginBypass(int index, bool bypass);
    void setBypassAll(bool bypass);
    bool isPluginBypassed(int index) const;
    bool isAllBypassed() const { return allBypassed.load(); }

    //=========================================================================
    // Master controls
    void setMasterBypass(bool bypass);
    bool isMasterBypassed() const { return masterBypassed.load(); }

    void setMasterVolume(float volume);
    float getMasterVolume() const { return masterVolume.load(); }

    //=========================================================================
    // State management
    ChainState getState() const;
    void setState(const ChainState& state);
    void saveState(const juce::File& file);
    bool loadState(const juce::File& file);

    //=========================================================================
    // CPU monitoring
    double getTotalCPUUsage() const;
    double getPluginCPUUsage(int index) const;

    //=========================================================================
    // Latency
    int getTotalLatencySamples() const;
    double getTotalLatencyMs() const;

    //=========================================================================
    // Callbacks
    void setChainChangeCallback(ChainChangeCallback callback);
    void setPluginLoadCallback(PluginLoadCallback callback);

    //=========================================================================
    // ARC specific features
    bool hasARCPlugin() const;
    int getARCPluginIndex() const;
    void setARCComparisonMode(bool abEnabled);
    bool isARCComparisonMode() const;

private:
    //=========================================================================
    void notifyChainChanged();
    void notifyPluginLoaded(int index, bool success, const juce::String& message);
    int findEmptySlot() const;
    void updateLatency();

    // Rebuild the RT-safe slot snapshot. Must be called (under write lock) any
    // time the chain topology changes: add, remove, move, bypass, prepareToPlay.
    void rebuildSnapshot();

    //=========================================================================
    // RT-safe snapshot — avoids shared_mutex and std::vector in processBlock.
    //
    // Two fixed-size snapshots are ping-ponged:
    //   Non-RT: builds into snapshots_[stagingIdx_], then atomically publishes.
    //   RT:     loads activeSnapshot_, iterates — zero locks, zero allocation.
    //
    // Crash flag array: if a plugin throws in the RT callback it sets the flag
    // here and the Non-RT timer removes it cleanly.
    struct SlotSnapshot
    {
        struct Entry { VSTPlugin* plugin; int slotIndex; };
        std::array<Entry, MAX_PLUGINS> entries{};
        int count{0};
    };

    std::array<SlotSnapshot, 2>    snapshots_{};
    std::atomic<SlotSnapshot*>     activeSnapshot_{nullptr};
    int                            stagingIdx_{0}; // non-RT bookkeeping

    // Per-slot crash flags: set by RT callback, cleared by Non-RT cleanup timer.
    std::array<std::atomic<bool>, MAX_PLUGINS> crashFlags_{};

    //=========================================================================
    PluginHost& pluginHost;

    std::vector<PluginSlot> slots;
    mutable std::shared_mutex chainMutex; // Use shared_mutex for read-write access

    std::atomic<bool> masterBypassed{false};
    std::atomic<bool> allBypassed{false};
    std::atomic<float> masterVolume{1.0f};

    juce::LinearSmoothedValue<float> masterVolumeSmoothed;

    double currentSampleRate{44100.0};
    int currentBlockSize{512};
    int totalLatencySamples{0};

    // Plugin Delay Compensation engine
    PDCEngine pdcEngine;

    // ARC comparison
    std::atomic<bool> arcComparisonMode{false};
    int arcPluginIndex{-1};

    // Callbacks
    ChainChangeCallback chainChangeCallback;
    PluginLoadCallback pluginLoadCallback;
    std::mutex callbackMutex;

    // Thread pool for async plugin loading
    juce::ThreadPool loadThreadPool{1};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VSTChain)
};

} // namespace ARC

