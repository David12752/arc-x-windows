/**
 * Electron preload script — runs in an isolated context with access to
 * both the Node.js APIs and the renderer DOM.
 *
 * Exposes a minimal, typed API to the renderer via contextBridge.
 * The renderer accesses this as `window.arcAPI`.
 */

import { contextBridge, ipcRenderer } from 'electron'
import type { EngineCommand, EngineEvent } from '@shared/commands'
import type { MetricsFrame } from '@shared/MetricsFrame'

// ---------------------------------------------------------------------------
// Type for the metrics frame as received from main (spectrum is number[])
// ---------------------------------------------------------------------------
export type SerializedMetricsFrame = Omit<MetricsFrame, 'spectrum' | 'sequenceNumber' | 'timestampMs'> & {
  sequenceNumber: string   // BigInt serialized as string
  timestampMs:    string
  spectrum:       number[] // Float32Array serialized as Array
}

// ---------------------------------------------------------------------------
// Public API exposed to renderer
// ---------------------------------------------------------------------------
export const arcAPI = {
  /** Send a command to arc-engine.exe */
  sendCommand: (cmd: EngineCommand): void => {
    ipcRenderer.send('engine:sendCommand', cmd)
  },

  /** Subscribe to 60Hz metrics frames */
  onMetrics: (callback: (frame: SerializedMetricsFrame) => void): (() => void) => {
    const handler = (_event: Electron.IpcRendererEvent, frame: SerializedMetricsFrame) =>
      callback(frame)
    ipcRenderer.on('metrics:frame', handler)
    // Return cleanup function
    return () => ipcRenderer.removeListener('metrics:frame', handler)
  },

  /** Subscribe to engine events (plugin loaded, scan complete, etc.) */
  onEngineEvent: (callback: (event: EngineEvent) => void): (() => void) => {
    const handler = (_event: Electron.IpcRendererEvent, engineEvent: EngineEvent) =>
      callback(engineEvent)
    ipcRenderer.on('engine:event', handler)
    return () => ipcRenderer.removeListener('engine:event', handler)
  },
}

contextBridge.exposeInMainWorld('arcAPI', arcAPI)
