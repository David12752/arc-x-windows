/**
 * CommandClient — sends binary-framed commands to arc-engine.exe
 * via the Named Pipe \\.\pipe\arc-command.
 *
 * Connection strategy:
 *   - Lazy connect on first send attempt.
 *   - Auto-reconnect with exponential backoff (max 5s) when the pipe closes.
 *   - sendCommand() resolves immediately (fire-and-forget); the pipe is full-duplex
 *     but the engine never sends replies on the command pipe (replies go to arc-events).
 */

import net from 'net'
import { encodeCommand } from '@shared/protocol'
import type { EngineCommand } from '@shared/commands'

const PIPE_PATH = '\\\\.\\pipe\\arc-command'

export class CommandClient {
  private socket:         net.Socket | null = null
  private connecting      = false
  private reconnectDelay  = 500  // ms, doubles on each failure
  private stopped         = false
  private pendingQueue:   Buffer[] = []

  start(): void {
    this.stopped = false
    this.connect()
  }

  stop(): void {
    this.stopped = true
    this.pendingQueue = []
    if (this.socket) {
      this.socket.destroy()
      this.socket = null
    }
  }

  /** Enqueue a command. Sends immediately if connected; queues otherwise. */
  sendCommand(cmd: EngineCommand): void {
    const frame = encodeCommand(cmd)
    if (this.socket && !this.socket.destroyed) {
      this.socket.write(frame)
    } else {
      // Cap pending queue to avoid unbounded growth if engine is down
      if (this.pendingQueue.length < 64) {
        this.pendingQueue.push(frame)
      }
      if (!this.connecting) this.connect()
    }
  }

  private connect(): void {
    if (this.stopped || this.connecting) return
    this.connecting = true

    const sock = net.createConnection(PIPE_PATH)

    sock.once('connect', () => {
      this.socket = sock
      this.connecting = false
      this.reconnectDelay = 500

      // Drain queued commands
      const q = this.pendingQueue.splice(0)
      for (const frame of q) sock.write(frame)
    })

    sock.once('error', () => {
      // 'close' always fires after 'error' — reconnect is handled there.
      // Calling scheduleReconnect() here AND in 'close' would double-schedule
      // and cause exponential timer growth.
      sock.destroy()
    })

    sock.once('close', () => {
      this.socket = null
      this.connecting = false
      if (!this.stopped) this.scheduleReconnect()
    })
  }

  private scheduleReconnect(): void {
    if (this.stopped) return
    setTimeout(() => this.connect(), this.reconnectDelay)
    this.reconnectDelay = Math.min(this.reconnectDelay * 2, 5000)
  }
}
