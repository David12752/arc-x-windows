/*
 * ============================================================================
 * ARC System Audio Host - Engine Command Types
 *
 * All commands that can be sent from the UI (via Named Pipe) to the engine.
 * Each command is a small POD struct.  The aggregate is a std::variant so
 * the compiler enforces exhaustive handling in the dispatch visitor.
 *
 * WIRE PROTOCOL (Phase 1 — plain binary, no external deps)
 * ---------------------------------------------------------
 * Every message on the named pipe is framed as:
 *
 *   [uint32: MAGIC = 0x41524345 "ARCE"]
 *   [uint32: payloadLength]
 *   [uint8:  commandType  (EngineCommandType enum)]
 *   [payloadLength-1 bytes: command-specific payload]
 *
 * In Phase 3 this framing will be replaced with FlatBuffers, but the
 * EngineCommand variant types here remain unchanged — only the (de)serializer
 * changes.
 *
 * THREAD SAFETY
 * -------------
 * Commands are deserialized on the CommandServer thread (Non-RT), then
 * enqueued into an SPSCQueue<EngineCommand, 256>.
 * The RT audio callback drains the queue at the start of each block.
 * All command structs must therefore be trivially copyable.
 * ============================================================================
 */

#pragma once

#include <cstdint>
#include <variant>
#include <type_traits>

namespace ARC
{

//=============================================================================
// Wire type tags — stored as uint8 in the pipe frame
enum class EngineCommandType : uint8_t
{
    SetVolume         = 0x01,
    SetBypass         = 0x02,
    LoadPlugin        = 0x03,
    RemovePlugin      = 0x04,
    MovePlugin        = 0x05,
    SetPluginBypass   = 0x06,
    SetPluginMix      = 0x07,
    SetInputGain      = 0x08,
    SetOutputGain     = 0x09,
    ScanPlugins       = 0x0A,
    LoadPreset        = 0x0B,
    SavePreset        = 0x0C,
    SetSampleRate     = 0x0D,
    SetBufferSize     = 0x0E,
    SetOutputDevice   = 0x0F,
    RequestStatus     = 0x10,  // engine replies via arc-events pipe
    DetectVBCable     = 0x11,  // engine replies with vbcable_status event
    SetVBCableRouting = 0x12,  // apply or revert VB-Cable routing
    Shutdown          = 0xFF,
};

//=============================================================================
// Command payload structs — all POD, trivially copyable

/** Set master output volume. gainDb: -inf to +6. */
struct SetVolumeCmd
{
    float gainDb{0.0f};
};

/** Bypass/un-bypass the entire plugin chain. */
struct SetBypassCmd
{
    uint8_t bypass{0};  // 0 = active, 1 = bypassed
};

/** Load a plugin into a chain slot. Paths are null-terminated. */
struct LoadPluginCmd
{
    int32_t  slot{0};
    char     pluginId[256]{};   // unique identifier string
    char     pluginPath[512]{}; // filesystem path to .vst3 bundle
};

/** Remove the plugin from the given slot. */
struct RemovePluginCmd
{
    int32_t slot{0};
};

/** Swap two slots in the chain. */
struct MovePluginCmd
{
    int32_t fromSlot{0};
    int32_t toSlot{0};
};

/** Bypass/un-bypass a single plugin slot. */
struct SetPluginBypassCmd
{
    int32_t slot{0};
    uint8_t bypass{0};
};

/** Set wet/dry mix for a plugin slot (0.0 = dry, 1.0 = wet). */
struct SetPluginMixCmd
{
    int32_t slot{0};
    float   mix{1.0f};
};

/** Set chain input gain (dB). */
struct SetInputGainCmd
{
    float gainDb{0.0f};
};

/** Set chain output gain (dB). */
struct SetOutputGainCmd
{
    float gainDb{0.0f};
};

/** Trigger a background plugin scan on the given directory path. */
struct ScanPluginsCmd
{
    char path[512]{};
};

/** Load a preset file into the chain. */
struct LoadPresetCmd
{
    char path[512]{};
};

/** Save current chain state as a preset. */
struct SavePresetCmd
{
    char path[512]{};
    char name[128]{};
};

/** Change engine sample rate (non-RT — causes device re-open). */
struct SetSampleRateCmd
{
    double sampleRate{48000.0};
};

/** Change audio buffer size (non-RT). */
struct SetBufferSizeCmd
{
    int32_t bufferSize{256};
};

/** Change output device (non-RT). */
struct SetOutputDeviceCmd
{
    char deviceName[256]{};
};

/** Request a full status push on the events pipe. */
struct RequestStatusCmd {};

/** Ask the engine to shut down cleanly. */
struct ShutdownCmd {};

/** Request VB-Cable detection; engine replies with a vbcable_status event. */
struct DetectVBCableCmd {};

/** Apply (enable=1) or revert (enable=0) VB-Cable routing. */
struct SetVBCableRoutingCmd
{
    uint8_t enable{1};   // 1 = route through CABLE, 0 = revert to default
    uint8_t pad[3]{};
};

//=============================================================================
// Compile-time checks: every command type must be trivially copyable
// so it can safely cross the SPSC queue without locks.
static_assert(std::is_trivially_copyable_v<SetVolumeCmd>);
static_assert(std::is_trivially_copyable_v<LoadPluginCmd>);
static_assert(std::is_trivially_copyable_v<ScanPluginsCmd>);

//=============================================================================
/**
 * EngineCommand — the variant type dispatched from the pipe to the RT path.
 *
 * Usage in the RT callback:
 *
 *   EngineCommand cmd;
 *   while (commandQueue.try_dequeue(cmd))
 *   {
 *       std::visit([this](auto&& c){ applyCommand(c); }, cmd);
 *   }
 */
using EngineCommand = std::variant<
    SetVolumeCmd,
    SetBypassCmd,
    LoadPluginCmd,
    RemovePluginCmd,
    MovePluginCmd,
    SetPluginBypassCmd,
    SetPluginMixCmd,
    SetInputGainCmd,
    SetOutputGainCmd,
    ScanPluginsCmd,
    LoadPresetCmd,
    SavePresetCmd,
    SetSampleRateCmd,
    SetBufferSizeCmd,
    SetOutputDeviceCmd,
    RequestStatusCmd,
    ShutdownCmd,
    DetectVBCableCmd,
    SetVBCableRoutingCmd
>;

// The variant must also be trivially copyable for SPSC usage.
// This will fail to compile if any member is non-trivial.
static_assert(std::is_trivially_copyable_v<EngineCommand>,
              "EngineCommand must be trivially copyable for SPSC queue.");

} // namespace ARC
