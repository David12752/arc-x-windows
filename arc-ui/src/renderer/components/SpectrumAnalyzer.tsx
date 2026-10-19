/**
 * SpectrumAnalyzer — 64-bin log-scale spectrum rendered via Canvas 2D.
 * Smooth bar display with peak-hold per bin.
 * Data arrives from MetricsFrame.spectrum[] at 60Hz.
 */

import { useEffect, useRef } from 'react'
import { useEngineStore } from '../store/useEngineStore'
import { SPECTRUM_BINS } from '@shared/MetricsFrame'

const W = 162   // total canvas width (pixels)
const H = 80    // total canvas height (pixels)

// Peak-hold state per bin (persists across renders)
const peakHold    = new Float32Array(SPECTRUM_BINS)
const peakDecay   = 0.98  // per-frame decay factor

export function SpectrumAnalyzer(): JSX.Element {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const metrics   = useEngineStore((s) => s.metrics)

  useEffect(() => {
    const canvas = canvasRef.current
    if (!canvas) return
    const ctx = canvas.getContext('2d')
    if (!ctx) return

    ctx.clearRect(0, 0, W, H)
    ctx.fillStyle = '#0d0d14'
    ctx.fillRect(0, 0, W, H)

    const bins    = metrics?.spectrum ?? null
    const binW    = W / SPECTRUM_BINS
    const grad    = ctx.createLinearGradient(0, H, 0, 0)
    grad.addColorStop(0,    '#00d67a')
    grad.addColorStop(0.6,  '#ffa500')
    grad.addColorStop(1,    '#ff4444')

    for (let i = 0; i < SPECTRUM_BINS; i++) {
      const mag  = bins ? Math.min(1, Math.max(0, bins[i])) : 0
      const barH = mag * H

      // Decay peak-hold
      peakHold[i] *= peakDecay
      if (mag > peakHold[i]) peakHold[i] = mag

      const x = i * binW
      ctx.fillStyle = grad
      ctx.fillRect(x + 1, H - barH, binW - 1, barH)

      // Peak-hold tick
      if (peakHold[i] > 0.02) {
        ctx.fillStyle = '#e0e0e8'
        ctx.fillRect(x + 1, H - peakHold[i] * H - 1, binW - 1, 1)
      }
    }

    // Grid lines (dB guides)
    ctx.strokeStyle = '#1e1e2e'
    ctx.lineWidth = 1
    for (const y of [0.25, 0.5, 0.75]) {
      ctx.beginPath()
      ctx.moveTo(0, y * H)
      ctx.lineTo(W, y * H)
      ctx.stroke()
    }
  }, [metrics])

  return (
    <div className="flex flex-col items-center gap-1">
      <span className="text-xs text-arc-muted font-mono">SPECTRUM</span>
      <canvas
        ref={canvasRef}
        width={W}
        height={H}
        className="rounded w-full"
      />
      <div className="flex justify-between w-full px-1">
        <span className="text-[9px] text-arc-muted font-mono">20Hz</span>
        <span className="text-[9px] text-arc-muted font-mono">1kHz</span>
        <span className="text-[9px] text-arc-muted font-mono">20kHz</span>
      </div>
    </div>
  )
}
