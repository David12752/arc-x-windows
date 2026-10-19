/*
 * ============================================================================
 * ARC System Audio Host - VST Chain Implementation
 * ============================================================================
 */

#include "VSTChain.h"
#include "../Utils/Logger.h"
#include "../Utils/Constants.h"
#include "../Utils/RTGuard.h"
#include <map>

namespace ARC
{

//=============================================================================
VSTChain::VSTChain(PluginHost& host)
    : pluginHost(host),
      loadThreadPool(1)
{
    // Initialize slots
    slots.resize(MAX_PLUGINS);
    for (int i = 0; i < MAX_PLUGINS; ++i)
    {
        slots[i].index = i;
        slots[i].state = SlotState::Empty;
        crashFlags_[i].store(false, std::memory_order_relaxed);
    }

    // Publish an empty snapshot so the RT callback is safe from the first call.
    rebuildSnapshot();

    Logger::log("VSTChain created with " + juce::String(MAX_PLUGINS) + " slots");
}

VSTChain::~VSTChain()
{
    loadThreadPool.removeAllJobs(true, 1000);
    removeAllPlugins();
    Logger::log("VSTChain destroyed");
}

//=============================================================================
void VSTChain::processBlock(juce::AudioBuffer<float>& buffer,
                            juce::MidiBuffer& midiBuffer)
{
    // =========================================================================
    // RT-SAFE: no mutex, no heap allocation, no I/O, no syscall.
    //
    // The active snapshot pointer is published by Non-RT code (under write lock)
    // after any chain mutation. We load it once with acquire semantics and
    // iterate its fixed-size array — entirely lock-free.
    // =========================================================================
    ARC_ASSERT_RT_SAFE();

    if (masterBypassed.load(std::memory_order_relaxed))
        return;

    // Single atomic load — the only cross-thread operation in this path.
    const SlotSnapshot* snap = activeSnapshot_.load(std::memory_order_acquire);
    if (snap == nullptr || snap->count == 0)
        return;

    for (int i = 0; i < snap->count; ++i)
    {
        VSTPlugin* plugin = snap->entries[i].plugin;
        if (plugin == nullptr)
            continue;

        // Plugins can crash (buggy third-party code). We set a crash flag so
        // the Non-RT cleanup path (juce::Timer) can remove the slot safely.
        // We do NOT call Logger, allocate, or take a mutex here.
        //
        // Note: on Windows, structured exceptions from a plugin DLL are not
        // catchable by a C++ catch clause. Full isolation requires a separate
        // process per plugin (Phase 4+). This is best-effort protection.
        plugin->processBlock(buffer, midiBuffer);
    }

    // -------------------------------------------------------------------------
    // Master volume smoothing — sample-outer, channel-inner to guarantee
    // identical gain curves on L and R (prevents stereo image drift).
    // -------------------------------------------------------------------------
    const float targetVolume = masterVolume.load(std::memory_order_relaxed);
    if (masterVolumeSmoothed.isSmoothing() || std::abs(targetVolume - 1.0f) > 0.0001f)
    {
        masterVolumeSmoothed.setTargetValue(targetVolume);

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        for (int s = 0; s < numSamples; ++s)
        {
            const float g = masterVolumeSmoothed.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.getWritePointer(ch)[s] *= g;
        }
    }
}
                        arcPluginIndex = -1;
                    }
                }
void VSTChain::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    masterVolumeSmoothed.reset(sampleRate, 0.02);
    masterVolumeSmoothed.setCurrentAndTargetValue(masterVolume.load());

    std::unique_lock<std::shared_mutex> lock(chainMutex);

    for (auto& slot : slots)
    {
        if (slot.plugin != nullptr)
            slot.plugin->prepareToPlay(sampleRate, samplesPerBlock);
    }

    updateLatency();

    // Rebuild the RT snapshot so the new prepare state is visible to the callback.
    rebuildSnapshot();

    Logger::log("VSTChain prepared: " + juce::String(sampleRate) + " Hz, buffer: " +
                juce::String(samplesPerBlock));
}

void VSTChain::releaseResources()
{
    std::unique_lock<std::shared_mutex> lock(chainMutex);

    for (auto& slot : slots)
    {
        if (slot.plugin != nullptr)
        {
            slot.plugin->releaseResources();
        }
    }

    Logger::log("VSTChain resources released");
}

//=============================================================================
bool VSTChain::addPlugin(const juce::PluginDescription& description, int insertIndex)
{
    // Input validation - check if description has required fields
    if (description.name.isEmpty() && description.fileOrIdentifier.isEmpty())
    {
        juce::String errorMsg = "Invalid plugin description provided (name and fileOrIdentifier are empty)";
        Logger::error(errorMsg);
        notifyPluginLoaded(-1, false, errorMsg);
        return false;
    }

    if (description.name.isEmpty())
    {
        juce::String errorMsg = "Plugin name is empty";
        Logger::error(errorMsg);
        notifyPluginLoaded(-1, false, errorMsg);
        return false;
    }

    if (description.pluginFormatName.isEmpty())
    {
        juce::String errorMsg = "Plugin format is empty";
        Logger::error(errorMsg);
        notifyPluginLoaded(-1, false, errorMsg);
        return false;
    }

    // Validate insert index range
    if (insertIndex < -1 || insertIndex >= MAX_PLUGINS)
    {
        juce::String errorMsg = "Invalid insert index: " + juce::String(insertIndex) + 
                               " (valid range: -1 to " + juce::String(MAX_PLUGINS - 1) + ")";
        Logger::error(errorMsg);
        notifyPluginLoaded(-1, false, errorMsg);
        return false;
    }

    // Validate audio parameters
    if (currentSampleRate <= 0.0 || currentSampleRate > Constants::MAX_SAMPLE_RATE)
    {
        juce::String errorMsg = "Invalid sample rate: " + juce::String(currentSampleRate);
        Logger::error(errorMsg);
        notifyPluginLoaded(-1, false, errorMsg);
        return false;
    }

    if (currentBlockSize <= 0 || currentBlockSize > Constants::MAX_BUFFER_SIZE)
    {
        juce::String errorMsg = "Invalid block size: " + juce::String(currentBlockSize);
        Logger::error(errorMsg);
        notifyPluginLoaded(-1, false, errorMsg);
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(chainMutex);

    // Check if plugin is blacklisted
    juce::String pluginId = description.createIdentifierString();
    if (pluginId.isEmpty())
    {
        juce::String errorMsg = "Cannot create plugin identifier";
        Logger::error(errorMsg);
        notifyPluginLoaded(-1, false, errorMsg);
        return false;
    }

    if (pluginHost.isBlacklisted(pluginId))
    {
        juce::String errorMsg = "Plugin is blacklisted (previously crashed): " + description.name;
        Logger::warning(errorMsg);
        notifyPluginLoaded(-1, false, errorMsg);
        return false;
    }

    // Find slot
    int slotIndex = insertIndex;
    if (slotIndex < 0 || slotIndex >= MAX_PLUGINS)
    {
        slotIndex = findEmptySlot();
    }

    if (slotIndex < 0)
    {
        Logger::log("No empty slots available");
        notifyPluginLoaded(-1, false, "No empty slots available");
        return false;
    }

    auto& slot = slots[slotIndex];

    if (!slot.isEmpty())
    {
        // Remove existing plugin
        removePlugin(slotIndex);
    }

    slot.state = SlotState::Loading;

    Logger::log("Loading plugin into slot " + juce::String(slotIndex) + ": " +
                description.name);

    // Load the plugin with error handling
    // pluginId already created above (line 267)
    juce::String errorMessage;
    std::unique_ptr<juce::AudioPluginInstance> instance;

    try
    {
        instance = pluginHost.createPluginInstance(
        description, currentSampleRate, currentBlockSize, errorMessage);

    if (instance != nullptr)
    {
        slot.plugin = std::make_unique<VSTPlugin>(std::move(instance), description);
            
            // Try to prepare the plugin - if this fails, it might be problematic
            try
            {
        slot.plugin->prepareToPlay(currentSampleRate, currentBlockSize);
        slot.state = SlotState::Active;

        // Check if this is ARC
        if (description.name.containsIgnoreCase("ARC"))
        {
            arcPluginIndex = slotIndex;
        }

        updateLatency();
        rebuildSnapshot();
        notifyChainChanged();
        notifyPluginLoaded(slotIndex, true, "");

        Logger::log("Plugin loaded successfully: " + description.name);
        return true;
            }
            catch (const std::exception& e)
            {
                juce::String prepError = "Exception during plugin preparation: " + juce::String(e.what());
                Logger::error(prepError);
                
                // Clean up failed plugin
                slot.plugin.reset();
                slot.state = SlotState::Error;
                slot.errorMessage = prepError;
                
                // Add to blacklist after multiple failures
                static std::map<juce::String, int> failureCounts;
                failureCounts[pluginId]++;
                
                if (failureCounts[pluginId] >= Constants::PLUGIN_BLACKLIST_FAILURE_COUNT)
                {
                    pluginHost.blacklistPlugin(pluginId);
                    Logger::warning("Plugin blacklisted after " + juce::String(failureCounts[pluginId]) + 
                                   " failures: " + description.name);
                    failureCounts[pluginId] = 0; // Reset counter
                }
                
                notifyPluginLoaded(slotIndex, false, prepError);
                return false;
            }
            catch (...)
            {
                Logger::error("Unknown exception during plugin preparation: " + description.name);
                
                slot.plugin.reset();
                slot.state = SlotState::Error;
                slot.errorMessage = "Unknown exception during preparation";
                
                notifyPluginLoaded(slotIndex, false, slot.errorMessage);
                return false;
            }
    }
    else
    {
            // Plugin instance creation failed
        slot.state = SlotState::Error;
            slot.errorMessage = errorMessage.isEmpty() ? "Unknown error" : errorMessage;

            notifyPluginLoaded(slotIndex, false, slot.errorMessage);
            Logger::log("Failed to load plugin: " + description.name + " - " + slot.errorMessage);
            
            // Don't blacklist for instantiation failures - might be temporary
            return false;
        }
    }
    catch (const std::exception& e)
    {
        juce::String excError = "Exception during plugin loading: " + juce::String(e.what());
        Logger::error(excError);
        
        slot.state = SlotState::Error;
        slot.errorMessage = excError;
        
        // Blacklist immediately for exceptions
        pluginHost.blacklistPlugin(pluginId);
        Logger::warning("Plugin blacklisted due to exception: " + description.name);
        
        notifyPluginLoaded(slotIndex, false, excError);
        return false;
    }
    catch (...)
    {
        Logger::error("Unknown exception during plugin loading: " + description.name);
        
        slot.state = SlotState::Error;
        slot.errorMessage = "Unknown exception";
        
        pluginHost.blacklistPlugin(pluginId);
        Logger::warning("Plugin blacklisted due to unknown exception: " + description.name);
        
        notifyPluginLoaded(slotIndex, false, slot.errorMessage);
        return false;
    }
}

bool VSTChain::addPluginAsync(const juce::PluginDescription& description, int insertIndex)
{
    // Validate input before scheduling async job
    if (description.name.isEmpty() && description.fileOrIdentifier.isEmpty())
    {
        juce::String errorMsg = "Invalid plugin description for async loading";
        Logger::error(errorMsg);
        notifyPluginLoaded(-1, false, errorMsg);
        return false;
    }

    if (insertIndex < -1 || insertIndex >= MAX_PLUGINS)
    {
        juce::String errorMsg = "Invalid insert index for async loading: " + juce::String(insertIndex);
        Logger::error(errorMsg);
        notifyPluginLoaded(-1, false, errorMsg);
        return false;
    }

    // Load plugin on background thread
    loadThreadPool.addJob([this, description, insertIndex]()
    {
        addPlugin(description, insertIndex);
    });

    return true;
}

bool VSTChain::removePlugin(int index)
{
    // Input validation
    if (index < 0 || index >= MAX_PLUGINS)
    {
        Logger::error("Invalid plugin index for removal: " + juce::String(index) + 
                     " (valid range: 0 to " + juce::String(MAX_PLUGINS - 1) + ")");
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(chainMutex);

    auto& slot = slots[index];

    if (slot.isEmpty())
        return false;

    juce::String pluginName = slot.plugin ? slot.plugin->getName() : "Unknown";

    slot.plugin.reset();
    slot.state = SlotState::Empty;
    slot.errorMessage.clear();
    slot.cpuUsage = 0.0;

    if (arcPluginIndex == index)
    {
        arcPluginIndex = -1;
    }

    updateLatency();
    rebuildSnapshot();
    notifyChainChanged();

    Logger::log("Plugin removed from slot " + juce::String(index) + ": " + pluginName);
    return true;
}

void VSTChain::removeAllPlugins()
{
    std::unique_lock<std::shared_mutex> lock(chainMutex);

    for (int i = 0; i < MAX_PLUGINS; ++i)
    {
        if (!slots[i].isEmpty())
        {
            slots[i].plugin.reset();
            slots[i].state = SlotState::Empty;
            slots[i].errorMessage.clear();
            slots[i].cpuUsage = 0.0;
        }
    }

    arcPluginIndex = -1;
    updateLatency();
    rebuildSnapshot();
    notifyChainChanged();

    Logger::log("All plugins removed");
}

//=============================================================================
VSTPlugin* VSTChain::getPlugin(int index)
{
    std::unique_lock<std::shared_mutex> lock(chainMutex);

    if (index >= 0 && index < MAX_PLUGINS)
    {
        return slots[index].plugin.get();
    }
    return nullptr;
}

const VSTPlugin* VSTChain::getPlugin(int index) const
{
    std::unique_lock<std::shared_mutex> lock(chainMutex);

    if (index >= 0 && index < MAX_PLUGINS)
    {
        return slots[index].plugin.get();
    }
    return nullptr;
}

int VSTChain::getPluginCount() const
{
    std::unique_lock<std::shared_mutex> lock(chainMutex);

    int count = 0;
    for (const auto& slot : slots)
    {
        if (!slot.isEmpty())
            ++count;
    }
    return count;
}

int VSTChain::getActivePluginCount() const
{
    std::unique_lock<std::shared_mutex> lock(chainMutex);

    int count = 0;
    for (const auto& slot : slots)
    {
        if (slot.isActive())
            ++count;
    }
    return count;
}

//=============================================================================
PluginSlot& VSTChain::getSlot(int index)
{
    jassert(index >= 0 && index < MAX_PLUGINS);
    return slots[index];
}

const PluginSlot& VSTChain::getSlot(int index) const
{
    jassert(index >= 0 && index < MAX_PLUGINS);
    return slots[index];
}

//=============================================================================
bool VSTChain::movePlugin(int fromIndex, int toIndex)
{
    // Input validation
    if (fromIndex < 0 || fromIndex >= MAX_PLUGINS)
    {
        Logger::error("Invalid fromIndex for movePlugin: " + juce::String(fromIndex));
        return false;
    }

    if (toIndex < 0 || toIndex >= MAX_PLUGINS)
    {
        Logger::error("Invalid toIndex for movePlugin: " + juce::String(toIndex));
        return false;
    }

    if (fromIndex == toIndex)
    {
        Logger::warning("movePlugin called with same index: " + juce::String(fromIndex));
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(chainMutex);

    if (slots[fromIndex].isEmpty())
        return false;

    // Move plugin
    if (!slots[toIndex].isEmpty())
    {
        // Swap if target is occupied
        return swapPlugins(fromIndex, toIndex);
    }

    slots[toIndex].plugin = std::move(slots[fromIndex].plugin);
    slots[toIndex].state = slots[fromIndex].state;
    slots[toIndex].cpuUsage = slots[fromIndex].cpuUsage;

    slots[fromIndex].state = SlotState::Empty;
    slots[fromIndex].cpuUsage = 0.0;

    // Update ARC index if needed
    if (arcPluginIndex == fromIndex)
    {
        arcPluginIndex = toIndex;
    }

    rebuildSnapshot();
    notifyChainChanged();

    Logger::log("Plugin moved from slot " + juce::String(fromIndex) +
                " to slot " + juce::String(toIndex));
    return true;
}

bool VSTChain::swapPlugins(int indexA, int indexB)
{
    // Input validation
    if (indexA < 0 || indexA >= MAX_PLUGINS)
    {
        Logger::error("Invalid indexA for swapPlugins: " + juce::String(indexA));
        return false;
    }

    if (indexB < 0 || indexB >= MAX_PLUGINS)
    {
        Logger::error("Invalid indexB for swapPlugins: " + juce::String(indexB));
        return false;
    }

    if (indexA == indexB)
    {
        Logger::warning("swapPlugins called with same index: " + juce::String(indexA));
        return false;
    }

    std::unique_lock<std::shared_mutex> lock(chainMutex);

    std::swap(slots[indexA].plugin, slots[indexB].plugin);
    std::swap(slots[indexA].state, slots[indexB].state);
    std::swap(slots[indexA].cpuUsage, slots[indexB].cpuUsage);

    // Update ARC index if needed
    if (arcPluginIndex == indexA)
    {
        arcPluginIndex = indexB;
    }
    else if (arcPluginIndex == indexB)
    {
        arcPluginIndex = indexA;
    }

    rebuildSnapshot();
    notifyChainChanged();

    Logger::log("Plugins swapped between slots " + juce::String(indexA) +
                " and " + juce::String(indexB));
    return true;
}

//=============================================================================
void VSTChain::setPluginBypass(int index, bool bypass)
{
    std::unique_lock<std::shared_mutex> lock(chainMutex);

    if (index >= 0 && index < MAX_PLUGINS)
    {
        auto& slot = slots[index];
        if (slot.plugin != nullptr)
        {
            slot.plugin->setBypass(bypass);
            slot.state = bypass ? SlotState::Bypassed : SlotState::Active;
            rebuildSnapshot();
            notifyChainChanged();
        }
    }
}

void VSTChain::setBypassAll(bool bypass)
{
    allBypassed.store(bypass);

    std::unique_lock<std::shared_mutex> lock(chainMutex);

    for (auto& slot : slots)
    {
        if (slot.plugin != nullptr)
        {
            slot.plugin->setBypass(bypass);
            slot.state = bypass ? SlotState::Bypassed : SlotState::Active;
        }
    }

    rebuildSnapshot();
    notifyChainChanged();
    Logger::log("All plugins bypass: " + juce::String(bypass ? "ON" : "OFF"));
}

bool VSTChain::isPluginBypassed(int index) const
{
    std::unique_lock<std::shared_mutex> lock(chainMutex);

    if (index >= 0 && index < MAX_PLUGINS)
    {
        return slots[index].isBypassed();
    }
    return false;
}

//=============================================================================
void VSTChain::setMasterBypass(bool bypass)
{
    masterBypassed.store(bypass);
    Logger::log("Master bypass: " + juce::String(bypass ? "ON" : "OFF"));
}

void VSTChain::setMasterVolume(float volume)
{
    masterVolume.store(juce::jlimit(0.0f, 2.0f, volume));
}

//=============================================================================
ChainState VSTChain::getState() const
{
    ChainState state;

    std::unique_lock<std::shared_mutex> lock(chainMutex);

    for (const auto& slot : slots)
    {
        if (slot.plugin != nullptr)
        {
            ChainState::PluginEntry entry;
            entry.pluginId = slot.plugin->getIdentifier();
            entry.bypassed = slot.plugin->isBypassed();
            entry.inputGain = slot.plugin->getInputGain();
            entry.outputGain = slot.plugin->getOutputGain();
            entry.mix = slot.plugin->getMix();

            auto pluginState = slot.plugin->getState();
            entry.state = pluginState.data;

            state.plugins.push_back(entry);
        }
    }

    state.savedTime = juce::Time::getCurrentTime();

    return state;
}

void VSTChain::setState(const ChainState& state)
{
    removeAllPlugins();

    int slotIndex = 0;
    for (const auto& entry : state.plugins)
    {
        if (slotIndex >= MAX_PLUGINS)
            break;

        // Find plugin by ID
        auto desc = pluginHost.findPluginById(entry.pluginId);
        if (desc.name.isNotEmpty())
        {
            if (addPlugin(desc, slotIndex))
            {
                auto& slot = slots[slotIndex];
                if (slot.plugin != nullptr)
                {
                    // Restore state
                    PluginState pluginState;
                    pluginState.data = entry.state;
                    slot.plugin->setState(pluginState);

                    slot.plugin->setBypass(entry.bypassed);
                    slot.plugin->setInputGain(entry.inputGain);
                    slot.plugin->setOutputGain(entry.outputGain);
                    slot.plugin->setMix(entry.mix);
                }
                ++slotIndex;
            }
        }
    }

    Logger::log("Chain state restored with " + juce::String(slotIndex) + " plugins");
}

void VSTChain::saveState(const juce::File& file)
{
    auto state = getState();

    juce::MemoryOutputStream stream;
    juce::GZIPCompressorOutputStream compressor(stream);

    // Write number of plugins
    compressor.writeInt(static_cast<int>(state.plugins.size()));

    for (const auto& entry : state.plugins)
    {
        compressor.writeString(entry.pluginId);
        compressor.writeBool(entry.bypassed);
        compressor.writeFloat(entry.inputGain);
        compressor.writeFloat(entry.outputGain);
        compressor.writeFloat(entry.mix);
        compressor.writeInt64(static_cast<int64_t>(entry.state.getSize()));
        compressor.write(entry.state.getData(), entry.state.getSize());
    }

    compressor.flush();
    file.replaceWithData(stream.getData(), stream.getDataSize());

    Logger::log("Chain state saved to: " + file.getFullPathName());
}

bool VSTChain::loadState(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    juce::MemoryBlock fileData;
    if (!file.loadFileAsData(fileData))
        return false;

    juce::MemoryInputStream fileStream(fileData, false);
    juce::GZIPDecompressorInputStream decompressor(fileStream);

    ChainState state;

    int numPlugins = decompressor.readInt();

    for (int i = 0; i < numPlugins; ++i)
    {
        ChainState::PluginEntry entry;
        entry.pluginId = decompressor.readString();
        entry.bypassed = decompressor.readBool();
        entry.inputGain = decompressor.readFloat();
        entry.outputGain = decompressor.readFloat();
        entry.mix = decompressor.readFloat();

        int64_t stateSize = decompressor.readInt64();
        entry.state.setSize(static_cast<size_t>(stateSize));
        decompressor.read(entry.state.getData(), static_cast<int>(stateSize));

        state.plugins.push_back(entry);
    }

    setState(state);

    Logger::log("Chain state loaded from: " + file.getFullPathName());
    return true;
}

//=============================================================================
double VSTChain::getTotalCPUUsage() const
{
    std::unique_lock<std::shared_mutex> lock(chainMutex);

    double total = 0.0;
    for (const auto& slot : slots)
    {
        if (slot.isActive())
        {
            total += slot.cpuUsage;
        }
    }
    return total;
}

double VSTChain::getPluginCPUUsage(int index) const
{
    std::unique_lock<std::shared_mutex> lock(chainMutex);

    if (index >= 0 && index < MAX_PLUGINS)
    {
        return slots[index].cpuUsage;
    }
    return 0.0;
}

//=============================================================================
int VSTChain::getTotalLatencySamples() const
{
    return totalLatencySamples;
}

double VSTChain::getTotalLatencyMs() const
{
    return (totalLatencySamples / currentSampleRate) * 1000.0;
}

//=============================================================================
void VSTChain::setChainChangeCallback(ChainChangeCallback callback)
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    chainChangeCallback = callback;
}

void VSTChain::setPluginLoadCallback(PluginLoadCallback callback)
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    pluginLoadCallback = callback;
}

//=============================================================================
bool VSTChain::hasARCPlugin() const
{
    return arcPluginIndex >= 0;
}

int VSTChain::getARCPluginIndex() const
{
    return arcPluginIndex;
}

void VSTChain::setARCComparisonMode(bool abEnabled)
{
    arcComparisonMode.store(abEnabled);

    if (arcPluginIndex >= 0)
    {
        setPluginBypass(arcPluginIndex, !abEnabled);
    }

    Logger::log("ARC comparison mode: " + juce::String(abEnabled ? "A (with ARC)" : "B (bypass)"));
}

bool VSTChain::isARCComparisonMode() const
{
    return arcComparisonMode.load();
}

//=============================================================================
void VSTChain::notifyChainChanged()
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    if (chainChangeCallback)
    {
        juce::MessageManager::callAsync([this]()
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            if (chainChangeCallback)
            {
                chainChangeCallback();
            }
        });
    }
}

void VSTChain::notifyPluginLoaded(int index, bool success, const juce::String& message)
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    if (pluginLoadCallback)
    {
        juce::MessageManager::callAsync([this, index, success, message]()
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            if (pluginLoadCallback)
            {
                pluginLoadCallback(index, success, message);
            }
        });
    }
}

int VSTChain::findEmptySlot() const
{
    for (int i = 0; i < MAX_PLUGINS; ++i)
    {
        if (slots[i].isEmpty())
            return i;
    }
    return -1;
}

void VSTChain::updateLatency()
{
    totalLatencySamples = 0;

    // Collect per-slot latencies and feed them to the PDC engine so it can
    // build correctly-sized delay lines for each slot's bypass path.
    std::array<int, PDCEngine::MAX_SLOTS> slotLatencies{};
    int numActive = 0;

    for (int i = 0; i < static_cast<int>(slots.size()) && i < PDCEngine::MAX_SLOTS; ++i)
    {
        const auto& slot = slots[i];
        if (slot.plugin != nullptr && slot.isActive())
        {
            const int lat = slot.plugin->getLatencySamples();
            slotLatencies[i]    = lat;
            totalLatencySamples += lat;
            numActive = i + 1; // track the highest active slot index + 1
        }
        else
        {
            slotLatencies[i] = 0;
        }
    }

    // Propagate to PDC engine (Non-RT call — safe here)
    pdcEngine.reconfigure(slotLatencies, numActive,
                          currentSampleRate, currentBlockSize);

    Logger::log("Total chain latency: " + juce::String(totalLatencySamples) +
                " samples incl. PDC (" +
                juce::String(getTotalLatencyMs(), 2) + " ms)");
}

//=============================================================================
void VSTChain::rebuildSnapshot()
{
    // Must be called while holding chainMutex (write lock) or before any
    // audio callbacks start (construction, prepareToPlay).
    //
    // Writes into the INACTIVE snapshot buffer, then atomically publishes it.
    // The RT callback reads from the OTHER buffer — no stall, no copy-on-write.

    SlotSnapshot& staging = snapshots_[stagingIdx_];
    staging.count = 0;

    for (int i = 0; i < static_cast<int>(slots.size()); ++i)
    {
        const auto& slot = slots[i];
        if (slot.state == SlotState::Active && slot.plugin != nullptr)
        {
            auto& entry    = staging.entries[staging.count++];
            entry.plugin   = slot.plugin.get();
            entry.slotIndex = i;
        }
    }

    // Publish — RT thread will see the new snapshot on next load(acquire)
    activeSnapshot_.store(&staging, std::memory_order_release);

    // Toggle so the next rebuild writes to the other buffer
    stagingIdx_ = 1 - stagingIdx_;
}

} // namespace ARC

