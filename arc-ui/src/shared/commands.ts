/**
 * Engine command types — mirrors EngineCommandType enum from EngineCommand.h.
 * These are the uint8 wire tags used in the binary pipe protocol.
 */
export const enum CommandType {
  SetVolume       = 0x01,
  SetBypass       = 0x02,
  LoadPlugin      = 0x03,
  RemovePlugin    = 0x04,
  MovePlugin      = 0x05,
  SetPluginBypass = 0x06,
  SetPluginMix    = 0x07,
  SetInputGain    = 0x08,
  SetOutputGain   = 0x09,
  ScanPlugins     = 0x0A,
  LoadPreset      = 0x0B,
  SavePreset      = 0x0C,
  SetSampleRate   = 0x0D,
  SetBufferSize   = 0x0E,
  SetOutputDevice = 0x0F,
  RequestStatus     = 0x10,
  DetectVBCable     = 0x11,
  SetVBCableRouting = 0x12,
  Shutdown          = 0xFF,
}

// Discriminated union of all command types (used by the UI)
export type EngineCommand =
  | { type: CommandType.SetVolume;       gainDb: number }
  | { type: CommandType.SetBypass;       bypass: boolean }
  | { type: CommandType.LoadPlugin;      slot: number; pluginId: string; pluginPath: string }
  | { type: CommandType.RemovePlugin;    slot: number }
  | { type: CommandType.MovePlugin;      fromSlot: number; toSlot: number }
  | { type: CommandType.SetPluginBypass; slot: number; bypass: boolean }
  | { type: CommandType.SetPluginMix;    slot: number; mix: number }
  | { type: CommandType.SetInputGain;    gainDb: number }
  | { type: CommandType.SetOutputGain;   gainDb: number }
  | { type: CommandType.ScanPlugins;     path: string }
  | { type: CommandType.LoadPreset;      path: string }
  | { type: CommandType.SavePreset;      path: string; name: string }
  | { type: CommandType.SetSampleRate;   sampleRate: number }
  | { type: CommandType.SetBufferSize;   bufferSize: number }
  | { type: CommandType.SetOutputDevice; deviceName: string }
  | { type: CommandType.RequestStatus }
  | { type: CommandType.DetectVBCable }
  | { type: CommandType.SetVBCableRouting; enable: boolean }
  | { type: CommandType.Shutdown }

/**
 * Engine event — pushed by the engine on \\.\pipe\arc-events as JSON.
 */
export type EngineEvent =
  | { event: 'connected';      version: string }
  | { event: 'status';         engineRunning: boolean; sampleRate: number; bufferSize: number }
  | { event: 'pluginLoaded';   slot: number; name: string; latency: number }
  | { event: 'pluginError';    slot: number; message: string }
  | { event: 'scanComplete';   plugins: ScannedPlugin[] }
  | { event: 'xrun';           count: number }
  | { event: 'vbcable_status'; installed: boolean; renderDevice: string; captureDevice: string }
  | { event: 'routing_applied'; device: 'vbcable' | 'default' }
  | { event: 'error';          message: string }
  | { event: 'shutdown' }

export interface ScannedPlugin {
  id:           string
  name:         string
  manufacturer: string
  path:         string
  category:     string
  numInputs:    number
  numOutputs:   number
  latencySamples: number
}
