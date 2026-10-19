/**
 * MetricsFrame — TypeScript mirror of the C++ MetricsFrame POD struct.
 * Returned by the native shm_reader addon's readFrame() call.
 *
 * All float fields are JS numbers (float64 holding float32 values).
 * Keep in sync with src/Concurrency/MetricsFrame.h.
 */
export interface MetricsFrame {
  // Identity
  sequenceNumber:         bigint
  timestampMs:            bigint

  // Levels (linear 0.0–1.0, 1.0 = 0 dBFS)
  peakL:                  number
  peakR:                  number
  rmsL:                   number
  rmsR:                   number
  peakHoldL:              number
  peakHoldR:              number

  // Limiter
  limiterGainReductionDb: number // 0 = no reduction, negative = attenuating

  // CPU
  cpuPercent:             number
  cpuPeakPercent:         number

  // Latency
  totalLatencySamples:    number
  totalLatencyMs:         number
  xrunCount:              number

  // Engine state
  engineRunning:          boolean
  masterBypassed:         boolean
  srcActive:              boolean
  activePluginCount:      number

  // Device
  engineSampleRate:       number
  loopbackSampleRate:     number
  bufferSize:             number

  // Per-slot (10 slots)
  slotCpuPercent:         number[]  // length 10
  slotBypassed:           boolean[] // length 10

  // Spectrum (64 log-scaled bins, 0.0–1.0)
  spectrum:               Float32Array
}

export const SPECTRUM_BINS = 64
export const MAX_PLUGIN_SLOTS = 10

/**
 * Convert a linear level (0.0–1.0) to dBFS.
 * Returns -∞ (represented as -144) for silence.
 */
export function linearToDb(linear: number): number {
  if (linear <= 0) return -144
  return 20 * Math.log10(linear)
}

/**
 * Convert dBFS to a 0.0–1.0 meter position for display.
 * Range: -60 dBFS (bottom) to +6 dBFS (top).
 */
export function dbToMeterPos(db: number, minDb = -60, maxDb = 6): number {
  return Math.max(0, Math.min(1, (db - minDb) / (maxDb - minDb)))
}
