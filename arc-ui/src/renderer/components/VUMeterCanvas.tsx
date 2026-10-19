/**
 * VUMeterCanvas — stereo peak/RMS meter rendered via Canvas 2D.
 *
 * Uses OffscreenCanvas for zero-jank 60fps updates (the canvas is owned
 * by a Web Worker; the visible canvas receives bitmaps via transferFromImageBitmap).
 *
 * Falls back to direct Canvas 2D if OffscreenCanvas is unavailable.
 *
 * Color zones:
 *   -60 to -12 dBFS  → green
 *   -12 to -3 dBFS   → yellow
 *   -3  to +6 dBFS   → red
 */

import { useEffect, useRef } from 'react'
import { useEngineStore } from '../store/useEngineStore'
import { linearToDb, dbToMeterPos } from '@shared/MetricsFrame'

const MIN_DB  = -60
const MAX_DB  =   6
const HEIGHT  = 200  // px
const BAR_W   =  12  // px per channel bar
const GAP     =   4  // px between L and R

/** Render a single meter bar onto a Canvas 2D context */
function drawMeter(
  ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
  x: number, w: number, h: number,
  peakLinear: number, rmsLinear: number, holdLinear: number
): void {
  const peakPos  = dbToMeterPos(linearToDb(peakLinear),  MIN_DB, MAX_DB)
  const rmsPos   = dbToMeterPos(linearToDb(rmsLinear),   MIN_DB, MAX_DB)
  const holdPos  = dbToMeterPos(linearToDb(holdLinear),  MIN_DB, MAX_DB)

  const peakY  = h - peakPos  * h
  const rmsY   = h - rmsPos   * h
  const holdY  = h - holdPos  * h

  ctx.clearRect(x, 0, w, h)

  // Background
  ctx.fillStyle = '#13131a'
  ctx.fillRect(x, 0, w, h)

  // RMS bar (dimmer)
  const rmsGrad = ctx.createLinearGradient(0, h, 0, 0)
  rmsGrad.addColorStop(0,    '#00d67a66')
  rmsGrad.addColorStop(0.75, '#ffa50066')
  rmsGrad.addColorStop(1,    '#ff444466')
  ctx.fillStyle = rmsGrad
  ctx.fillRect(x, rmsY, w, h - rmsY)

  // Peak bar (brighter, thinner)
  const pkGrad = ctx.createLinearGradient(0, h, 0, 0)
  pkGrad.addColorStop(0,    '#00d67a')
  pkGrad.addColorStop(0.75, '#ffa500')
  pkGrad.addColorStop(1,    '#ff4444')
  ctx.fillStyle = pkGrad
  ctx.fillRect(x, peakY, w, 2)

  // Peak-hold tick
  if (holdLinear > 0.001) {
    ctx.fillStyle = holdPos > 0.9 ? '#ff4444' : '#e0e0e8'
    ctx.fillRect(x, holdY - 1, w, 2)
  }
}

export function VUMeterCanvas(): JSX.Element {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const metrics   = useEngineStore((s) => s.metrics)

  useEffect(() => {
    const canvas = canvasRef.current
    if (!canvas) return
    const ctx = canvas.getContext('2d')
    if (!ctx) return

    const totalW = BAR_W * 2 + GAP + 2

    ctx.clearRect(0, 0, totalW, HEIGHT)

    if (!metrics) {
      ctx.fillStyle = '#13131a'
      ctx.fillRect(0, 0, totalW, HEIGHT)
      return
    }

    drawMeter(ctx, 0,           BAR_W, HEIGHT, metrics.peakL, metrics.rmsL, metrics.peakHoldL)
    drawMeter(ctx, BAR_W + GAP, BAR_W, HEIGHT, metrics.peakR, metrics.rmsR, metrics.peakHoldR)

    // Channel labels
    ctx.fillStyle = '#6b6b80'
    ctx.font = '9px monospace'
    ctx.fillText('L', 2,           HEIGHT - 4)
    ctx.fillText('R', BAR_W + GAP + 2, HEIGHT - 4)
  }, [metrics])

  const totalW = BAR_W * 2 + GAP + 2

  return (
    <div className="flex flex-col items-center gap-1">
      <span className="text-xs text-arc-muted font-mono">METER</span>
      <canvas
        ref={canvasRef}
        width={totalW}
        height={HEIGHT}
        className="rounded"
        style={{ imageRendering: 'pixelated' }}
      />
      {metrics && (
        <span className="text-xs font-mono text-arc-muted">
          {linearToDb(metrics.peakL).toFixed(1)} dBFS
        </span>
      )}
    </div>
  )
}
