/**
 * PluginChainView — the 10-slot serial plugin chain with drag-reorder.
 *
 * Each slot shows:
 *   - Plugin name (or "Empty" with drop target styling)
 *   - Bypass toggle
 *   - Wet/Dry mix knob (0–100%)
 *   - Per-slot CPU %
 *   - Remove button
 *
 * Drag reorder uses HTML5 drag-and-drop (no extra library dependency).
 */

import { useRef, useState } from 'react'
import { useEngineStore } from '../store/useEngineStore'
import type { PluginSlot } from '../store/useEngineStore'

export function PluginChainView(): JSX.Element {
  const slots          = useEngineStore((s) => s.slots)
  const metrics        = useEngineStore((s) => s.metrics)
  const setPluginBypass = useEngineStore((s) => s.setPluginBypass)
  const setPluginMix    = useEngineStore((s) => s.setPluginMix)
  const removePlugin    = useEngineStore((s) => s.removePlugin)
  const movePlugin      = useEngineStore((s) => s.movePlugin)
  const setPanel        = useEngineStore((s) => s.setPanel)

  const dragIndex = useRef<number | null>(null)
  const [dragOver, setDragOver] = useState<number | null>(null)

  const handleDragStart = (idx: number) => { dragIndex.current = idx }
  const handleDragOver  = (e: React.DragEvent, idx: number) => {
    e.preventDefault()
    setDragOver(idx)
  }
  const handleDrop = (targetIdx: number) => {
    if (dragIndex.current !== null && dragIndex.current !== targetIdx)
      movePlugin(dragIndex.current, targetIdx)
    dragIndex.current = null
    setDragOver(null)
  }
  const handleDragEnd = () => {
    dragIndex.current = null
    setDragOver(null)
  }

  const activeCount  = slots.filter((s) => s.name).length
  const totalCpu     = metrics?.cpuPercent.toFixed(1) ?? '—'
  const totalLatency = metrics ? `${metrics.totalLatencyMs.toFixed(1)} ms` : '—'
  const xruns        = metrics?.xrunCount ?? 0

  return (
    <div className="flex flex-col h-full overflow-hidden">
      {/* Status bar */}
      <div className="flex items-center gap-4 px-4 py-2 border-b border-arc-border text-xs text-arc-muted font-mono shrink-0">
        <span>{activeCount}/10 plugins</span>
        <span>CPU: <span className="text-arc-text">{totalCpu}%</span></span>
        <span>Latency: <span className="text-arc-text">{totalLatency}</span></span>
        {xruns > 0 && (
          <span className="text-arc-red">Xruns: {xruns}</span>
        )}
      </div>

      {/* Slot list */}
      <div className="flex-1 overflow-y-auto px-3 py-2 space-y-1.5">
        {slots.map((slot, idx) => {
          const slotCpu    = metrics?.slotCpuPercent[idx] ?? 0
          const slotBypass = metrics?.slotBypassed[idx] ?? slot.bypassed
          const isEmpty    = !slot.name

          return (
            <div
              key={idx}
              draggable={!isEmpty}
              onDragStart={() => handleDragStart(idx)}
              onDragOver={(e) => handleDragOver(e, idx)}
              onDrop={() => handleDrop(idx)}
              onDragEnd={handleDragEnd}
              className={[
                'flex items-center gap-3 px-3 py-2 rounded border transition-colors',
                dragOver === idx
                  ? 'border-arc-accent bg-arc-accent/10'
                  : isEmpty
                    ? 'border-arc-border/50 bg-arc-surface/50 border-dashed'
                    : 'border-arc-border bg-arc-surface hover:border-arc-border/80',
                slotBypass && !isEmpty ? 'opacity-50' : '',
              ].join(' ')}
            >
              {/* Slot number */}
              <span className="text-xs text-arc-muted font-mono w-5 shrink-0">{idx + 1}</span>

              {/* Drag handle */}
              {!isEmpty && (
                <span className="text-arc-muted cursor-grab active:cursor-grabbing text-sm">⠿</span>
              )}

              {/* Plugin name / empty drop target */}
              {isEmpty ? (
                <button
                  className="flex-1 text-xs text-arc-muted/50 text-left italic"
                  onClick={() => setPanel('browser')}
                >
                  + Drop plugin here
                </button>
              ) : (
                <span className="flex-1 text-sm font-mono truncate">{slot.name}</span>
              )}

              {/* Per-slot CPU */}
              {!isEmpty && slotCpu > 0 && (
                <span className="text-xs text-arc-muted font-mono w-10 text-right">
                  {slotCpu.toFixed(1)}%
                </span>
              )}

              {/* Mix knob (slider) */}
              {!isEmpty && (
                <div className="flex items-center gap-1">
                  <span className="text-xs text-arc-muted font-mono w-6">
                    {Math.round(slot.mix * 100)}
                  </span>
                  <input
                    type="range" min={0} max={1} step={0.01}
                    value={slot.mix}
                    onChange={(e) => setPluginMix(idx, parseFloat(e.target.value))}
                    className="w-16 h-1 accent-arc-accent"
                    title="Wet/Dry mix"
                  />
                </div>
              )}

              {/* Bypass */}
              {!isEmpty && (
                <button
                  title={slotBypass ? 'Disabled' : 'Active'}
                  onClick={() => setPluginBypass(idx, !slotBypass)}
                  className={`text-xs px-2 py-0.5 rounded border transition-colors ${
                    slotBypass
                      ? 'border-arc-yellow text-arc-yellow bg-arc-yellow/10'
                      : 'border-arc-border text-arc-muted hover:border-arc-accent'
                  }`}
                >
                  B
                </button>
              )}

              {/* Remove */}
              {!isEmpty && (
                <button
                  onClick={() => removePlugin(idx)}
                  className="text-xs text-arc-muted hover:text-arc-red transition-colors px-1"
                  title="Remove plugin"
                >
                  ✕
                </button>
              )}
            </div>
          )
        })}
      </div>
    </div>
  )
}
