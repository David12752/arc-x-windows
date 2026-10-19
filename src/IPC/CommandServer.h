/*
 * ============================================================================
 * ARC System Audio Host - Named Pipe Command Server
 *
 * Listens on \\.\pipe\arc-command for inbound commands from the Electron UI.
 * Deserializes each command frame and enqueues it into the engine's
 * SPSCQueue<EngineCommand> for consumption by the RT audio callback.
 *
 * Thread model:
 *   - Server runs on its own dedicated Non-RT thread (backgroundThread_).
 *   - The RT audio thread only calls SPSCQueue::try_dequeue() — no pipe I/O.
 *   - Main thread calls start()/stop() which are thread-safe.
 *
 * Protocol (Phase 1 binary framing):
 *   [uint32: MAGIC 0x41524345]
 *   [uint32: payloadLength     ]
 *   [uint8:  commandType       ]
 *   [payloadLength-1: payload  ]
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"
#include "EngineCommand.h"
#include "../Concurrency/SPSCQueue.h"
#include <atomic>
#include <functional>
#include <thread>
#include <string>

namespace ARC
{

//=============================================================================
constexpr const char* ARC_COMMAND_PIPE_NAME = "\\\\.\\pipe\\arc-command";
constexpr const char* ARC_EVENTS_PIPE_NAME  = "\\\\.\\pipe\\arc-events";
constexpr uint32_t    ARC_PIPE_MAGIC        = 0x41524345u; // "ARCE"
constexpr uint32_t    ARC_MAX_PAYLOAD_BYTES = 4096;

//=============================================================================
/**
 * CommandServer — Named Pipe server for inbound UI commands.
 *
 * The server creates the pipe, accepts one client at a time, reads frames,
 * deserializes them into EngineCommand variants, and enqueues them into the
 * shared SPSCQueue so the RT callback can pick them up without any I/O.
 */
class CommandServer
{
public:
    //=========================================================================
    using CommandQueue = SPSCQueue<EngineCommand, 256>;

    /** Callback invoked (on the server thread) when the client connects. */
    using ConnectCallback    = std::function<void()>;
    /** Callback invoked (on the server thread) when the client disconnects. */
    using DisconnectCallback = std::function<void()>;

    //=========================================================================
    /**
     * @param queue  Reference to the SPSC queue that the RT thread reads from.
     *               The caller owns the queue lifetime.
     */
    explicit CommandServer(CommandQueue& queue);
    ~CommandServer();

    //=========================================================================
    /** Start the server thread. Safe to call from any thread. */
    bool start();

    /** Stop the server thread and close the pipe. Blocks until the thread exits. */
    void stop();

    bool isRunning() const noexcept { return running_.load(); }

    //=========================================================================
    /** Register callbacks for client connect/disconnect events. */
    void setConnectCallback(ConnectCallback cb)    { onConnect_    = std::move(cb); }
    void setDisconnectCallback(DisconnectCallback cb) { onDisconnect_ = std::move(cb); }

    //=========================================================================
    /**
     * Push an arbitrary event string to the Electron UI via the events pipe.
     * Called from Non-RT code (e.g. plugin scan progress, error notifications).
     * Thread-safe (uses an internal mutex for the events pipe write).
     */
    void pushEvent(const std::string& jsonPayload);

private:
    //=========================================================================
    void serverLoop();
    bool waitForClient();
    bool readFrame(void* pipeHandle);
    bool deserializeAndEnqueue(const uint8_t* payload, uint32_t length,
                                EngineCommandType type);

    //=========================================================================
    CommandQueue&      queue_;
    std::atomic<bool>  running_{false};
    std::thread        backgroundThread_;

    ConnectCallback    onConnect_;
    DisconnectCallback onDisconnect_;

    // Events pipe (push notifications engine → UI)
    void*              eventsPipeHandle_{nullptr};
    std::mutex         eventsMutex_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CommandServer)
};

} // namespace ARC
