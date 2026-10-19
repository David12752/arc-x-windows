/**
 * EventListener — reads JSON event objects pushed by arc-engine.exe
 * on the Named Pipe \\.\pipe\arc-events.
 *
 * The engine connects to this pipe as a client (it creates the pipe server side).
 * We connect as a client and listen for newline-delimited JSON objects.
 *
 * On each incoming event:
 *   1. Parse JSON → EngineEvent
 *   2. Forward to the renderer via BrowserWindow.webContents.send('engine:event', event)
 *   3. Emit via the onEvent callback (for main-process consumers like tray)
 *
 * Reconnect on close with the same backoff as CommandClient.
 */

import net from 'net'
import { BrowserWindow } from 'electron'
import type { EngineEvent } from '@shared/commands'

const PIPE_PATH = '\\\\.\\pipe\\arc-events'

export class EventListener {
  private socket:        net.Socket | null = null
  private window:        BrowserWindow | null = null
  private stopped        = false
  private connecting     = false
  private reconnectDelay = 500
  private buffer         = ''
  private onEvent?:      (event: EngineEvent) => void

  start(win: BrowserWindow, onEvent?: (event: EngineEvent) => void): void {
    this.window   = win
    this.onEvent  = onEvent
    this.stopped  = false
    this.connect()
  }

  stop(): void {
    this.stopped = true
    if (this.socket) {
      this.socket.destroy()
      this.socket = null
    }
  }

  private connect(): void {
    if (this.stopped || this.connecting) return
    this.connecting = true

    const sock = net.createConnection(PIPE_PATH)
    sock.setEncoding('utf8')

    sock.once('connect', () => {
      this.socket = sock
      this.reconnectDelay = 500
      this.buffer = ''
    })

    sock.on('data', (chunk: string) => {
      this.buffer += chunk
      // Events are newline-delimited JSON objects
      const lines = this.buffer.split('\n')
      this.buffer = lines.pop() ?? ''  // keep the incomplete last line

      for (const line of lines) {
        const trimmed = line.trim()
        if (!trimmed) continue
        try {
          const event = JSON.parse(trimmed) as EngineEvent
          this.dispatch(event)
        } catch {
          // Malformed JSON from engine — ignore
        }
      }
    })

    sock.once('error', () => {
      // 'close' always fires after 'error' — reconnect handled there only.
      sock.destroy()
    })

    sock.once('close', () => {
      this.socket = null
      this.connecting = false
      if (!this.stopped) this.scheduleReconnect()
    })
  }

  private dispatch(event: EngineEvent): void {
    if (this.window && !this.window.isDestroyed())
      this.window.webContents.send('engine:event', event)
    if (this.onEvent) this.onEvent(event)
  }

  private scheduleReconnect(): void {
    if (this.stopped) return
    setTimeout(() => this.connect(), this.reconnectDelay)
    this.reconnectDelay = Math.min(this.reconnectDelay * 2, 5000)
  }
}
