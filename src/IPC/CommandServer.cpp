/*
 * ============================================================================
 * ARC System Audio Host - Named Pipe Command Server Implementation
 * ============================================================================
 */

#include "CommandServer.h"
#include "../Utils/Logger.h"
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ARC
{

//=============================================================================
CommandServer::CommandServer(CommandQueue& queue)
    : queue_(queue)
{
    Logger::log("CommandServer created — pipe: " +
                juce::String(ARC_COMMAND_PIPE_NAME));
}

CommandServer::~CommandServer()
{
    stop();
}

//=============================================================================
bool CommandServer::start()
{
    if (running_.load())
        return true;

    running_.store(true);
    backgroundThread_ = std::thread(&CommandServer::serverLoop, this);

    Logger::log("CommandServer started");
    return true;
}

void CommandServer::stop()
{
    if (!running_.load())
        return;

    running_.store(false);

#ifdef _WIN32
    // Cancel any blocking ConnectNamedPipe / ReadFile by creating a dummy
    // client connection — unblocks the server thread cleanly.
    HANDLE dummy = CreateFileA(
        ARC_COMMAND_PIPE_NAME,
        GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, 0, nullptr);
    if (dummy != INVALID_HANDLE_VALUE)
        CloseHandle(dummy);
#endif

    if (backgroundThread_.joinable())
        backgroundThread_.join();

    Logger::log("CommandServer stopped");
}

//=============================================================================
void CommandServer::serverLoop()
{
    Logger::log("CommandServer: server thread started");

#ifdef _WIN32
    while (running_.load())
    {
        // Create the named pipe for commands (inbound)
        HANDLE hPipe = CreateNamedPipeA(
            ARC_COMMAND_PIPE_NAME,
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1,           // max instances
            4096,        // out buffer
            4096,        // in buffer
            0,           // default timeout
            nullptr);    // default security

        if (hPipe == INVALID_HANDLE_VALUE)
        {
            Logger::error("CommandServer: CreateNamedPipeA failed, error=" +
                          juce::String(static_cast<int>(GetLastError())));
            juce::Thread::sleep(1000);
            continue;
        }

        Logger::log("CommandServer: waiting for client connection...");

        // Block until a client connects
        BOOL connected = ConnectNamedPipe(hPipe, nullptr)
                        ? TRUE
                        : (GetLastError() == ERROR_PIPE_CONNECTED ? TRUE : FALSE);

        if (!running_.load())
        {
            CloseHandle(hPipe);
            break;
        }

        if (!connected)
        {
            Logger::warning("CommandServer: ConnectNamedPipe failed, error=" +
                            juce::String(static_cast<int>(GetLastError())));
            CloseHandle(hPipe);
            continue;
        }

        Logger::log("CommandServer: client connected");
        if (onConnect_)
            onConnect_();

        // Read frames until the client disconnects or we're told to stop
        while (running_.load())
        {
            if (!readFrame(hPipe))
                break; // client disconnected or error
        }

        Logger::log("CommandServer: client disconnected");
        if (onDisconnect_)
            onDisconnect_();

        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }
#else
    Logger::warning("CommandServer: Named Pipes are Windows-only in this build.");
#endif

    Logger::log("CommandServer: server thread exiting");
}

//=============================================================================
bool CommandServer::readFrame(void* pipeHandle)
{
#ifdef _WIN32
    HANDLE hPipe = static_cast<HANDLE>(pipeHandle);

    // --- Read header: magic(4) + payloadLength(4) = 8 bytes ---
    uint8_t header[8];
    DWORD   bytesRead = 0;

    if (!ReadFile(hPipe, header, sizeof(header), &bytesRead, nullptr) ||
        bytesRead != sizeof(header))
    {
        // Pipe closed or error — not worth logging every disconnect
        return false;
    }

    uint32_t magic, payloadLength;
    std::memcpy(&magic,         header,     4);
    std::memcpy(&payloadLength, header + 4, 4);

    if (magic != ARC_PIPE_MAGIC)
    {
        Logger::warning("CommandServer: bad magic 0x" +
                        juce::String::toHexString(static_cast<int>(magic)));
        return false;
    }

    if (payloadLength == 0 || payloadLength > ARC_MAX_PAYLOAD_BYTES)
    {
        Logger::warning("CommandServer: invalid payload length " +
                        juce::String(payloadLength));
        return false;
    }

    // --- Read payload ---
    std::vector<uint8_t> payload(payloadLength);
    DWORD totalRead = 0;

    while (totalRead < payloadLength)
    {
        DWORD chunk = 0;
        if (!ReadFile(hPipe, payload.data() + totalRead,
                      payloadLength - totalRead, &chunk, nullptr) ||
            chunk == 0)
            return false;
        totalRead += chunk;
    }

    // First byte of payload is the command type
    const auto cmdType = static_cast<EngineCommandType>(payload[0]);
    return deserializeAndEnqueue(payload.data() + 1, payloadLength - 1, cmdType);

#else
    juce::ignoreUnused(pipeHandle);
    return false;
#endif
}

//=============================================================================
bool CommandServer::deserializeAndEnqueue(const uint8_t* data, uint32_t length,
                                           EngineCommandType type)
{
    EngineCommand cmd;
    bool valid = true;

    // Helper: safe copy into a POD command struct
    auto safeCopy = [&](void* dst, std::size_t dstSize) -> bool
    {
        if (length < dstSize)
        {
            Logger::warning("CommandServer: payload too short for command type 0x" +
                            juce::String::toHexString(static_cast<int>(type)));
            return false;
        }
        std::memcpy(dst, data, dstSize);
        return true;
    };

    switch (type)
    {
        case EngineCommandType::SetVolume:
        {
            SetVolumeCmd c{};
            valid = safeCopy(&c, sizeof(c));
            cmd = c;
            break;
        }
        case EngineCommandType::SetBypass:
        {
            SetBypassCmd c{};
            valid = safeCopy(&c, sizeof(c));
            cmd = c;
            break;
        }
        case EngineCommandType::LoadPlugin:
        {
            LoadPluginCmd c{};
            valid = safeCopy(&c, sizeof(c));
            cmd = c;
            break;
        }
        case EngineCommandType::RemovePlugin:
        {
            RemovePluginCmd c{};
            valid = safeCopy(&c, sizeof(c));
            cmd = c;
            break;
        }
        case EngineCommandType::MovePlugin:
        {
            MovePluginCmd c{};
            valid = safeCopy(&c, sizeof(c));
            cmd = c;
            break;
        }
        case EngineCommandType::SetPluginBypass:
        {
            SetPluginBypassCmd c{};
            valid = safeCopy(&c, sizeof(c));
            cmd = c;
            break;
        }
        case EngineCommandType::SetPluginMix:
        {
            SetPluginMixCmd c{};
            valid = safeCopy(&c, sizeof(c));
            cmd = c;
            break;
        }
        case EngineCommandType::SetInputGain:
        {
            SetInputGainCmd c{};
            valid = safeCopy(&c, sizeof(c));
            cmd = c;
            break;
        }
        case EngineCommandType::SetOutputGain:
        {
            SetOutputGainCmd c{};
            valid = safeCopy(&c, sizeof(c));
            cmd = c;
            break;
        }
        case EngineCommandType::ScanPlugins:
        {
            ScanPluginsCmd c{};
            valid = safeCopy(&c, sizeof(c));
            cmd = c;
            break;
        }
        case EngineCommandType::LoadPreset:
        {
            LoadPresetCmd c{};
            valid = safeCopy(&c, sizeof(c));
            cmd = c;
            break;
        }
        case EngineCommandType::SavePreset:
        {
            SavePresetCmd c{};
            valid = safeCopy(&c, sizeof(c));
            cmd = c;
            break;
        }
        case EngineCommandType::SetSampleRate:
        {
            SetSampleRateCmd c{};
            valid = safeCopy(&c, sizeof(c));
            cmd = c;
            break;
        }
        case EngineCommandType::SetBufferSize:
        {
            SetBufferSizeCmd c{};
            valid = safeCopy(&c, sizeof(c));
            cmd = c;
            break;
        }
        case EngineCommandType::SetOutputDevice:
        {
            SetOutputDeviceCmd c{};
            valid = safeCopy(&c, sizeof(c));
            cmd = c;
            break;
        }
        case EngineCommandType::RequestStatus:
            cmd = RequestStatusCmd{};
            break;
        case EngineCommandType::Shutdown:
            cmd = ShutdownCmd{};
            break;
        default:
            Logger::warning("CommandServer: unknown command type 0x" +
                            juce::String::toHexString(static_cast<int>(type)));
            return false;
    }

    if (!valid)
        return false;

    // Enqueue into SPSC — if full, drop (Non-RT side is responsible for
    // not flooding the engine; the UI should rate-limit commands)
    if (!queue_.try_enqueue(cmd))
    {
        Logger::warning("CommandServer: command queue full, dropping command type 0x" +
                        juce::String::toHexString(static_cast<int>(type)));
    }

    return true;
}

//=============================================================================
void CommandServer::pushEvent(const std::string& jsonPayload)
{
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(eventsMutex_);

    if (eventsPipeHandle_ == nullptr || eventsPipeHandle_ == INVALID_HANDLE_VALUE)
    {
        // Lazy-connect to the events pipe (client side — Electron creates the server)
        eventsPipeHandle_ = CreateFileA(
            ARC_EVENTS_PIPE_NAME,
            GENERIC_WRITE, 0, nullptr,
            OPEN_EXISTING, 0, nullptr);

        if (eventsPipeHandle_ == INVALID_HANDLE_VALUE)
        {
            eventsPipeHandle_ = nullptr; // retry next time
            return;
        }
    }

    // Frame: [magic][length][payload]
    const uint32_t magic   = ARC_PIPE_MAGIC;
    const uint32_t payLen  = static_cast<uint32_t>(jsonPayload.size());

    DWORD written = 0;
    WriteFile(eventsPipeHandle_, &magic,  4,      &written, nullptr);
    WriteFile(eventsPipeHandle_, &payLen, 4,      &written, nullptr);
    WriteFile(eventsPipeHandle_, jsonPayload.data(), payLen, &written, nullptr);
#else
    juce::ignoreUnused(jsonPayload);
#endif
}

} // namespace ARC
