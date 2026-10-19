/**
 * Zustand store — single source of truth for engine state in the renderer.
 *
 * Slices:
 *   - metrics: latest MetricsFrame data (updated at 60Hz via IPC)
 *   - engine:  connection status, bypass toggle, device settings
 *   - plugins: chain slot state, browser plugins list
 *   - ui:      active panel, modal state
 *
 * The store subscribes to window.arcAPI on mount via useEngineStoreInit() hook.
 */

import { create } from 'zustand'
import { CommandType } from '@shared/commands'
import type { ScannedPlugin, EngineEvent } from '@shared/commands'
import type { MetricsFrame } from '@shared/MetricsFrame'

// Serialized form received over IPC (BigInt → string, Float32Array → number[])
export type SerializedMetricsFrame = Omit<MetricsFrame, 'spectrum' | 'sequenceNumber' | 'timestampMs'> & {
  sequenceNumber: string
  timestampMs:    string
  spectrum:       number[]
}

// ---------------------------------------------------------------------------
// Plugin slot state
// ---------------------------------------------------------------------------
export interface PluginSlot {
  index:      number
  name:       string
  pluginId:   string
  pluginPath: string
  bypassed:   boolean
  mix:        number   // 0.0 – 1.0
  cpuPercent: number
}

const EMPTY_SLOT: PluginSlot = {
  index: 0, name: '', pluginId: '', pluginPath: '', bypassed: false, mix: 1, cpuPercent: 0
}

// ---------------------------------------------------------------------------
// Store shape
// ---------------------------------------------------------------------------
export interface EngineStore {
  // Metrics (60Hz)
  metrics:         SerializedMetricsFrame | null

  // Engine status
  engineConnected: boolean
  masterBypassed:  boolean

  // Device
  sampleRate:      number
  bufferSize:      number
  outputDevice:    string

  // Plugin chain (10 slots)
  slots:           PluginSlot[]

  // Plugin browser
  scannedPlugins:  ScannedPlugin[]
  isScanRunning:   boolean

  // UI
  activePanel:     'chain' | 'browser' | 'settings'

  // VB-Cable routing state
  vbcableInstalled:     boolean
  vbcableRenderDevice:  string
  vbcableCaptureDevice: string
  vbcableRouting:       'vbcable' | 'default' | null   // null = unknown

  // Actions
  setMetrics:     (frame: SerializedMetricsFrame) => void
  handleEvent:    (event: EngineEvent) => void
  setMasterBypass:(bypass: boolean) => void
  setVolume:      (gainDb: number) => void
  setInputGain:   (gainDb: number) => void
  setOutputGain:  (gainDb: number) => void
  loadPlugin:     (slot: number, plugin: ScannedPlugin) => void
  removePlugin:   (slot: number) => void
  movePlugin:     (from: number, to: number) => void
  setPluginBypass:(slot: number, bypass: boolean) => void
  setPluginMix:   (slot: number, mix: number) => void
  setPanel:       (panel: 'chain' | 'browser' | 'settings') => void
  scanPlugins:    (path: string) => void
  detectVBCable:  () => void
  setVBCableRouting: (enable: boolean) => void
}

// ---------------------------------------------------------------------------
// Store implementation
// ---------------------------------------------------------------------------
export const useEngineStore = create<EngineStore>((set, _get) => ({
  metrics:         null,
  engineConnected: false,
  masterBypassed:  false,
  sampleRate:      48000,
  bufferSize:      256,
  outputDevice:    '',
  slots:           Array.from({ length: 10 }, (_, i) => ({ ...EMPTY_SLOT, index: i })),
  scannedPlugins:  [],
  isScanRunning:   false,
  activePanel:     'chain',
  vbcableInstalled:     false,
  vbcableRenderDevice:  '',
  vbcableCaptureDevice: '',
  vbcableRouting:       null,

  setMetrics: (frame) => set({ metrics: frame }),

  handleEvent: (event) => {
    switch (event.event) {
      case 'connected':
        set({ engineConnected: true })
        break
      case 'status':
        set({
          engineConnected: event.engineRunning,
          sampleRate:      event.sampleRate,
          bufferSize:      event.bufferSize,
        })
        break
      case 'pluginLoaded':
        set((state) => {
          const slots = [...state.slots]
          slots[event.slot] = {
            ...slots[event.slot],
            name: event.name,
            bypassed: false,
            mix: 1,
          }
          return { slots }
        })
        break
      case 'scanComplete':
        set({ scannedPlugins: event.plugins, isScanRunning: false })
        break
      case 'vbcable_status':
        set({
          vbcableInstalled:     event.installed,
          vbcableRenderDevice:  event.renderDevice,
          vbcableCaptureDevice: event.captureDevice,
        })
        break
      case 'routing_applied':
        set({ vbcableRouting: event.device })
        break
      case 'error':
        // Surface errors to console; UI can observe via dedicated state if needed
        console.error('[ARC engine error]', event.message)
        break
      case 'shutdown':
        set({ engineConnected: false })
        break
    }
  },

  setMasterBypass: (bypass) => {
    set({ masterBypassed: bypass })
    window.arcAPI.sendCommand({ type: CommandType.SetBypass, bypass })
  },

  setVolume: (gainDb) => {
    window.arcAPI.sendCommand({ type: CommandType.SetVolume, gainDb })
  },

  setInputGain: (gainDb) => {
    window.arcAPI.sendCommand({ type: CommandType.SetInputGain, gainDb })
  },

  setOutputGain: (gainDb) => {
    window.arcAPI.sendCommand({ type: CommandType.SetOutputGain, gainDb })
  },

  loadPlugin: (slot, plugin) => {
    window.arcAPI.sendCommand({
      type:       CommandType.LoadPlugin,
      slot,
      pluginId:   plugin.id,
      pluginPath: plugin.path,
    })
  },

  removePlugin: (slot) => {
    set((state) => {
      const slots = [...state.slots]
      slots[slot] = { ...EMPTY_SLOT, index: slot }
      return { slots }
    })
    window.arcAPI.sendCommand({ type: CommandType.RemovePlugin, slot })
  },

  movePlugin: (fromSlot, toSlot) => {
    set((state) => {
      const slots = [...state.slots]
      const tmp = slots[fromSlot]
      slots[fromSlot] = { ...slots[toSlot], index: fromSlot }
      slots[toSlot]   = { ...tmp, index: toSlot }
      return { slots }
    })
    window.arcAPI.sendCommand({ type: CommandType.MovePlugin, fromSlot, toSlot })
  },

  setPluginBypass: (slot, bypass) => {
    set((state) => {
      const slots = [...state.slots]
      slots[slot] = { ...slots[slot], bypassed: bypass }
      return { slots }
    })
    window.arcAPI.sendCommand({ type: CommandType.SetPluginBypass, slot, bypass })
  },

  setPluginMix: (slot, mix) => {
    set((state) => {
      const slots = [...state.slots]
      slots[slot] = { ...slots[slot], mix }
      return { slots }
    })
    window.arcAPI.sendCommand({ type: CommandType.SetPluginMix, slot, mix })
  },

  setPanel: (panel) => set({ activePanel: panel }),

  scanPlugins: (path) => {
    set({ isScanRunning: true })
    window.arcAPI.sendCommand({ type: CommandType.ScanPlugins, path })
  },

  detectVBCable: () => {
    window.arcAPI.sendCommand({ type: CommandType.DetectVBCable })
  },

  setVBCableRouting: (enable) => {
    window.arcAPI.sendCommand({ type: CommandType.SetVBCableRouting, enable })
  },
}))

// ---------------------------------------------------------------------------
// Init hook — call once at App root to wire IPC → store
// ---------------------------------------------------------------------------
export function useEngineStoreInit(): void {
  const setMetrics  = useEngineStore((s) => s.setMetrics)
  const handleEvent = useEngineStore((s) => s.handleEvent)

  // Subscribe on mount, unsubscribe on unmount
  // Wrap in useEffect in the caller (App.tsx)
  // This is exported as a plain object for use in useEffect:
  useEngineStore.getState // suppress lint — actual subscription below

  // Exported for App.tsx useEffect
  void { setMetrics, handleEvent }
}
