/**
 * SettingsPanel — device and engine configuration.
 *
 * Controls:
 *   - Output device (text input — list comes from engine status event)
 *   - Sample rate (dropdown: 44100, 48000, 88200, 96000)
 *   - Buffer size (dropdown: 64, 128, 256, 512, 1024)
 *   - Master output gain slider (-24 to +6 dB)
 *   - Input gain slider (-24 to +6 dB)
 *   - CPU / latency status display
 */

import { useState } from 'react'
import { useEngineStore } from '../store/useEngineStore'
import { CommandType } from '@shared/commands'
const SAMPLE_RATES = [44100, 48000, 88200, 96000]
const BUFFER_SIZES = [64, 128, 256, 512, 1024]

export function SettingsPanel(): JSX.Element {
  const metrics       = useEngineStore((s) => s.metrics)
  const sampleRate    = useEngineStore((s) => s.sampleRate)
  const bufferSize    = useEngineStore((s) => s.bufferSize)
  const outputDevice  = useEngineStore((s) => s.outputDevice)
  const setInputGain  = useEngineStore((s) => s.setInputGain)
  const setOutputGain = useEngineStore((s) => s.setOutputGain)

  // VB-Cable state
  const vbcableInstalled     = useEngineStore((s) => s.vbcableInstalled)
  const vbcableRenderDevice  = useEngineStore((s) => s.vbcableRenderDevice)
  const vbcableCaptureDevice = useEngineStore((s) => s.vbcableCaptureDevice)
  const vbcableRouting       = useEngineStore((s) => s.vbcableRouting)
  const detectVBCable        = useEngineStore((s) => s.detectVBCable)
  const setVBCableRouting    = useEngineStore((s) => s.setVBCableRouting)

  const [deviceInput,   setDeviceInput]   = useState(outputDevice)
  const [outputGain,    setOutputGainVal]  = useState(0)
  const [inputGain,     setInputGainVal]   = useState(0)

  return (
    <div className="h-full overflow-y-auto px-6 py-4 space-y-6">
      <h2 className="text-sm font-mono text-arc-accent uppercase tracking-widest">Settings</h2>

      {/* ─── Device ──────────────────────────────────────────────────────── */}
      <section>
        <h3 className="text-xs text-arc-muted font-mono uppercase mb-3">Audio Device</h3>
        <div className="space-y-3">
          <div className="flex items-center gap-3">
            <label className="text-xs text-arc-muted w-28 shrink-0">Output Device</label>
            <input
              type="text"
              value={deviceInput}
              onChange={(e) => setDeviceInput(e.target.value)}
              className="flex-1 bg-arc-surface border border-arc-border rounded px-3 py-1.5 text-sm text-arc-text focus:outline-none focus:border-arc-accent font-mono"
              placeholder="Default"
            />
            <button
              onClick={() =>
                window.arcAPI.sendCommand({ type: CommandType.SetOutputDevice, deviceName: deviceInput })
              }
              className="px-3 py-1.5 text-xs rounded border border-arc-border text-arc-muted hover:border-arc-accent hover:text-arc-accent transition-colors"
            >
              Apply
            </button>
          </div>

          <div className="flex items-center gap-3">
            <label className="text-xs text-arc-muted w-28 shrink-0">Sample Rate</label>
            <select
              value={sampleRate}
              onChange={(e) =>
                window.arcAPI.sendCommand({
                  type: CommandType.SetSampleRate,
                  sampleRate: parseInt(e.target.value)
                })
              }
              className="bg-arc-surface border border-arc-border rounded px-3 py-1.5 text-sm text-arc-text focus:outline-none focus:border-arc-accent"
            >
              {SAMPLE_RATES.map((r) => (
                <option key={r} value={r}>{r} Hz</option>
              ))}
            </select>
          </div>

          <div className="flex items-center gap-3">
            <label className="text-xs text-arc-muted w-28 shrink-0">Buffer Size</label>
            <select
              value={bufferSize}
              onChange={(e) =>
                window.arcAPI.sendCommand({
                  type: CommandType.SetBufferSize,
                  bufferSize: parseInt(e.target.value)
                })
              }
              className="bg-arc-surface border border-arc-border rounded px-3 py-1.5 text-sm text-arc-text focus:outline-none focus:border-arc-accent"
            >
              {BUFFER_SIZES.map((s) => (
                <option key={s} value={s}>{s} samples</option>
              ))}
            </select>
          </div>
        </div>
      </section>

      {/* ─── VB-Cable Routing ────────────────────────────────────────────── */}
      <section>
        <div className="flex items-center justify-between mb-3">
          <h3 className="text-xs text-arc-muted font-mono uppercase">Virtual Cable Routing</h3>
          <button
            onClick={detectVBCable}
            className="px-2 py-1 text-xs rounded border border-arc-border text-arc-muted hover:border-arc-accent hover:text-arc-accent transition-colors font-mono"
          >
            Rescan
          </button>
        </div>

        {/* Status badge */}
        <div className="flex items-center gap-2 mb-4">
          <span className={`inline-block w-2 h-2 rounded-full ${vbcableInstalled ? 'bg-green-400' : 'bg-arc-red'}`} />
          <span className={`text-xs font-mono ${vbcableInstalled ? 'text-green-400' : 'text-arc-red'}`}>
            {vbcableInstalled ? 'VB-Audio Virtual Cable detected' : 'VB-Audio Virtual Cable not installed'}
          </span>
        </div>

        {vbcableInstalled ? (
          <>
            {/* Routing diagram */}
            <div className="bg-arc-surface rounded p-3 mb-4 font-mono text-xs text-arc-muted overflow-x-auto whitespace-nowrap">
              <span className="text-arc-text">App audio</span>
              {' → '}
              <span className="text-arc-accent">{vbcableRenderDevice || 'CABLE Input'}</span>
              {' → '}
              <span className="text-arc-text">ARC Engine (loopback)</span>
              {' → '}
              <span className="text-arc-accent">{vbcableCaptureDevice || 'CABLE Output'}</span>
              {' → '}
              <span className="text-arc-text">Speakers</span>
            </div>

            {/* Apply / Revert buttons */}
            <div className="flex gap-2">
              <button
                onClick={() => setVBCableRouting(true)}
                disabled={vbcableRouting === 'vbcable'}
                className="flex-1 py-1.5 text-xs rounded border border-arc-accent text-arc-accent font-mono hover:bg-arc-accent hover:text-arc-bg transition-colors disabled:opacity-40 disabled:cursor-not-allowed"
              >
                {vbcableRouting === 'vbcable' ? '✓ Routing Active' : 'Apply VB-Cable Routing'}
              </button>
              <button
                onClick={() => setVBCableRouting(false)}
                disabled={vbcableRouting !== 'vbcable'}
                className="flex-1 py-1.5 text-xs rounded border border-arc-border text-arc-muted font-mono hover:border-arc-accent hover:text-arc-accent transition-colors disabled:opacity-40 disabled:cursor-not-allowed"
              >
                Revert to Default
              </button>
            </div>
          </>
        ) : (
          <div className="bg-arc-surface rounded p-3 space-y-2">
            <p className="text-xs text-arc-muted leading-relaxed">
              VB-Audio Virtual Cable creates a virtual audio device that lets ARC capture
              and process system audio in real time.
            </p>
            <a
              href="https://vb-audio.com/Cable/"
              target="_blank"
              rel="noopener noreferrer"
              className="inline-block text-xs text-arc-accent hover:underline font-mono"
            >
              ↗ Download VB-Audio Virtual Cable (free)
            </a>
            <p className="text-xs text-arc-muted">
              After installing, click <strong className="text-arc-text">Rescan</strong> above or restart ARC.
            </p>
          </div>
        )}
      </section>

      {/* ─── Gain ────────────────────────────────────────────────────────── */}
      <section>
        <h3 className="text-xs text-arc-muted font-mono uppercase mb-3">Gain</h3>
        <div className="space-y-4">
          <Slider
            label="Input Gain"
            value={inputGain}
            min={-24} max={6}
            unit="dB"
            onChange={(v) => { setInputGainVal(v); setInputGain(v) }}
          />
          <Slider
            label="Output Gain"
            value={outputGain}
            min={-24} max={6}
            unit="dB"
            onChange={(v) => { setOutputGainVal(v); setOutputGain(v) }}
          />
        </div>
      </section>

      {/* ─── Diagnostics ─────────────────────────────────────────────────── */}
      <section>
        <h3 className="text-xs text-arc-muted font-mono uppercase mb-3">Diagnostics</h3>
        <div className="grid grid-cols-2 gap-3">
          <Stat label="CPU"         value={metrics ? `${metrics.cpuPercent.toFixed(1)}%` : '—'} />
          <Stat label="Peak CPU"    value={metrics ? `${metrics.cpuPeakPercent.toFixed(1)}%` : '—'} />
          <Stat label="Latency"     value={metrics ? `${metrics.totalLatencyMs.toFixed(2)} ms` : '—'} />
          <Stat label="Buffer"      value={metrics ? `${metrics.bufferSize} smp` : '—'} />
          <Stat label="Engine SR"   value={metrics ? `${(metrics.engineSampleRate / 1000).toFixed(1)} kHz` : '—'} />
          <Stat label="Loopback SR" value={metrics ? `${(metrics.loopbackSampleRate / 1000).toFixed(1)} kHz` : '—'} />
          <Stat label="SRC Active"  value={metrics ? (metrics.srcActive ? 'YES' : 'NO') : '—'} />
          <Stat label="Xruns"       value={metrics ? `${metrics.xrunCount}` : '—'} warn={!!metrics && metrics.xrunCount > 0} />
          <Stat label="Limiter GR"  value={metrics ? `${metrics.limiterGainReductionDb.toFixed(1)} dB` : '—'} />
        </div>
      </section>
    </div>
  )
}

// ── Small helpers ──────────────────────────────────────────────────────────

function Slider({
  label, value, min, max, unit, onChange
}: {
  label: string; value: number; min: number; max: number; unit: string
  onChange: (v: number) => void
}): JSX.Element {
  return (
    <div className="flex items-center gap-3">
      <label className="text-xs text-arc-muted w-28 shrink-0">{label}</label>
      <input
        type="range" min={min} max={max} step={0.1}
        value={value}
        onChange={(e) => onChange(parseFloat(e.target.value))}
        className="flex-1 accent-arc-accent"
      />
      <span className="text-xs text-arc-text font-mono w-16 text-right">
        {value >= 0 ? '+' : ''}{value.toFixed(1)} {unit}
      </span>
    </div>
  )
}

function Stat({
  label, value, warn = false
}: {
  label: string; value: string; warn?: boolean
}): JSX.Element {
  return (
    <div className="bg-arc-surface rounded p-2">
      <p className="text-xs text-arc-muted">{label}</p>
      <p className={`text-sm font-mono mt-0.5 ${warn ? 'text-arc-red' : 'text-arc-text'}`}>{value}</p>
    </div>
  )
}
