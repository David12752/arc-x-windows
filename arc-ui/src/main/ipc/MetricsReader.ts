/**
 * MetricsReader — polls the arc_metrics_shm shared memory at 60 Hz
 * and broadcasts the latest MetricsFrame to the renderer via IPC.
 *
 * Uses the native shm_reader addon (shm_reader.node) for seqlock-consistent reads.
 * Falls back gracefully when the engine is not running (addon returns null).
 *
 * Architecture:
 *   Main process timer (16ms) → native readFrame() → webContents.send('metrics:frame', frame)
 *
 * The renderer listens via contextBridge exposure in the preload script.
 */

import { BrowserWindow } from 'electron'
import path from 'path'

// Load the pre-built native addon. In production it lives next to the app.
// In dev, electron-rebuild places it in native/build/Release/.
function loadAddon() {
  try {
    const addonPath = path.join(__dirname, '../../native/build/Release/shm_reader.node')
    // eslint-disable-next-line @typescript-eslint/no-require-imports
    return require(addonPath) as {
      open():      void
      close():     void
      isOpen():    boolean
      readFrame(): Record<string, unknown> | null
    }
  } catch {
    console.warn('[MetricsReader] Native addon not found — metrics unavailable.')
    return null
  }
}

const addon = loadAddon()

export class MetricsReader {
  private timer:   ReturnType<typeof setInterval> | null = null
  private window:  BrowserWindow | null = null
  private lastSeq: bigint = -1n

  start(win: BrowserWindow): void {
    this.window = win
    if (!addon) return

    addon.open()

    this.timer = setInterval(() => this.poll(), 16) // ~60 Hz
  }

  stop(): void {
    if (this.timer) {
      clearInterval(this.timer)
      this.timer = null
    }
    if (addon) addon.close()
    this.window = null
  }

  private poll(): void {
    if (!this.window || this.window.isDestroyed()) return
    if (!addon) return

    // Lazy re-open if the engine restarted
    if (!addon.isOpen()) addon.open()

    const frame = addon.readFrame()
    if (!frame) return

    // Skip duplicate frames (engine hasn't produced a new one yet)
    const seq = frame.sequenceNumber as bigint
    if (seq === this.lastSeq) return
    this.lastSeq = seq

    // Send to renderer. JSON-serialization handles most types;
    // spectrum (Float32Array) is sent as a plain Array for IPC compatibility.
    const serializable = {
      ...frame,
      sequenceNumber: String(seq),
      timestampMs:    String(frame.timestampMs as bigint),
      spectrum:       Array.from(frame.spectrum as Float32Array),
    }

    this.window.webContents.send('metrics:frame', serializable)
  }
}
