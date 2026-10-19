/**
 * PluginBrowser — virtualized list of scanned VST3 plugins.
 * Uses @tanstack/react-virtual for smooth scrolling of 1000+ items.
 *
 * Features:
 *   - Live search/filter (name + manufacturer)
 *   - Category filter chips
 *   - Double-click or drag to load into chain slot
 *   - Scan folder trigger with path input
 */

import { useState, useMemo, useRef } from 'react'
import { useVirtualizer } from '@tanstack/react-virtual'
import { useEngineStore } from '../store/useEngineStore'
import type { ScannedPlugin } from '@shared/commands'

const DEFAULT_SCAN_PATH = 'C:\\Program Files\\Common Files\\VST3'

export function PluginBrowser(): JSX.Element {
  const scannedPlugins = useEngineStore((s) => s.scannedPlugins)
  const isScanRunning  = useEngineStore((s) => s.isScanRunning)
  const loadPlugin     = useEngineStore((s) => s.loadPlugin)
  const scanPlugins    = useEngineStore((s) => s.scanPlugins)
  const slots          = useEngineStore((s) => s.slots)

  const [query,         setQuery]         = useState('')
  const [categoryFilter, setCategoryFilter] = useState<string | null>(null)
  const [scanPath,       setScanPath]       = useState(DEFAULT_SCAN_PATH)
  const [targetSlot,     setTargetSlot]     = useState(0)

  // Get unique categories
  const categories = useMemo(
    () => Array.from(new Set(scannedPlugins.map((p) => p.category).filter(Boolean))).sort(),
    [scannedPlugins]
  )

  // Filtered list
  const filtered = useMemo(() => {
    const q = query.toLowerCase()
    return scannedPlugins.filter((p) => {
      if (categoryFilter && p.category !== categoryFilter) return false
      if (!q) return true
      return p.name.toLowerCase().includes(q) || p.manufacturer.toLowerCase().includes(q)
    })
  }, [scannedPlugins, query, categoryFilter])

  // Virtualizer
  const parentRef = useRef<HTMLDivElement>(null)
  const virtualizer = useVirtualizer({
    count:         filtered.length,
    getScrollElement: () => parentRef.current,
    estimateSize:  () => 48,
    overscan:      10,
  })

  // Next empty slot
  const nextFreeSlot = slots.findIndex((s) => !s.name)

  const handleLoad = (plugin: ScannedPlugin) => {
    const slot = targetSlot < 10 ? targetSlot : (nextFreeSlot >= 0 ? nextFreeSlot : 0)
    loadPlugin(slot, plugin)
  }

  return (
    <div className="flex flex-col h-full">
      {/* Toolbar */}
      <div className="shrink-0 px-4 py-2 border-b border-arc-border space-y-2">
        {/* Search */}
        <div className="flex gap-2">
          <input
            type="text"
            placeholder="Search plugins…"
            value={query}
            onChange={(e) => setQuery(e.target.value)}
            className="flex-1 bg-arc-surface border border-arc-border rounded px-3 py-1.5 text-sm text-arc-text placeholder:text-arc-muted focus:outline-none focus:border-arc-accent"
          />
          <span className="text-xs text-arc-muted self-center font-mono">
            {filtered.length} / {scannedPlugins.length}
          </span>
        </div>

        {/* Category chips */}
        {categories.length > 0 && (
          <div className="flex flex-wrap gap-1">
            <button
              onClick={() => setCategoryFilter(null)}
              className={`text-xs px-2 py-0.5 rounded-full border transition-colors ${
                !categoryFilter ? 'border-arc-accent text-arc-accent' : 'border-arc-border text-arc-muted hover:border-arc-accent/50'
              }`}
            >
              All
            </button>
            {categories.map((cat) => (
              <button
                key={cat}
                onClick={() => setCategoryFilter(categoryFilter === cat ? null : cat)}
                className={`text-xs px-2 py-0.5 rounded-full border transition-colors ${
                  categoryFilter === cat ? 'border-arc-accent text-arc-accent' : 'border-arc-border text-arc-muted hover:border-arc-accent/50'
                }`}
              >
                {cat}
              </button>
            ))}
          </div>
        )}

        {/* Scan row */}
        <div className="flex gap-2">
          <input
            type="text"
            value={scanPath}
            onChange={(e) => setScanPath(e.target.value)}
            className="flex-1 bg-arc-surface border border-arc-border rounded px-3 py-1 text-xs text-arc-muted focus:outline-none focus:border-arc-accent font-mono"
            placeholder="Scan path…"
          />
          <select
            value={targetSlot}
            onChange={(e) => setTargetSlot(parseInt(e.target.value))}
            className="bg-arc-surface border border-arc-border rounded px-2 py-1 text-xs text-arc-muted focus:outline-none"
            title="Target chain slot"
          >
            {Array.from({ length: 10 }, (_, i) => (
              <option key={i} value={i}>→ Slot {i + 1}</option>
            ))}
          </select>
          <button
            onClick={() => scanPlugins(scanPath)}
            disabled={isScanRunning}
            className="px-3 py-1 text-xs rounded border border-arc-accent text-arc-accent hover:bg-arc-accent/10 transition-colors disabled:opacity-50 disabled:cursor-wait"
          >
            {isScanRunning ? 'Scanning…' : 'Scan'}
          </button>
        </div>
      </div>

      {/* Virtualized list */}
      <div ref={parentRef} className="flex-1 overflow-y-auto">
        {scannedPlugins.length === 0 ? (
          <div className="flex flex-col items-center justify-center h-full text-arc-muted gap-2">
            <span className="text-4xl">🔌</span>
            <p className="text-sm">No plugins found.</p>
            <p className="text-xs">Click Scan to search for VST3 plugins.</p>
          </div>
        ) : filtered.length === 0 ? (
          <div className="flex flex-col items-center justify-center h-full text-arc-muted gap-2">
            <p className="text-sm">No plugins match your search.</p>
            <button
              onClick={() => { setQuery(''); setCategoryFilter(null) }}
              className="text-xs text-arc-accent hover:underline"
            >
              Clear filters
            </button>
          </div>
        ) : (
          <div
            style={{ height: virtualizer.getTotalSize() }}
            className="relative w-full"
          >
            {virtualizer.getVirtualItems().map((vItem) => {
              const plugin = filtered[vItem.index]
              return (
                <div
                  key={vItem.key}
                  style={{
                    position:  'absolute',
                    top:       0,
                    left:      0,
                    width:     '100%',
                    height:    `${vItem.size}px`,
                    transform: `translateY(${vItem.start}px)`,
                  }}
                >
                  <div
                    className="w-full h-full flex items-center gap-3 px-4 text-left hover:bg-arc-surface border-b border-arc-border/30 transition-colors cursor-pointer"
                    onDoubleClick={() => handleLoad(plugin)}
                    title="Double-click to load"
                  >
                    <div className="flex-1 min-w-0">
                      <p className="text-sm truncate font-mono">{plugin.name}</p>
                      <p className="text-xs text-arc-muted truncate">{plugin.manufacturer}</p>
                    </div>
                    <div className="text-right shrink-0">
                      <p className="text-xs text-arc-muted">{plugin.category}</p>
                      <p className="text-xs text-arc-muted font-mono">
                        {plugin.numInputs}in/{plugin.numOutputs}out
                      </p>
                    </div>
                    <button
                      onClick={(e) => { e.stopPropagation(); handleLoad(plugin) }}
                      className="ml-2 text-xs px-2 py-1 rounded border border-arc-border hover:border-arc-accent hover:text-arc-accent text-arc-muted transition-colors shrink-0"
                    >
                      Load
                    </button>
                  </div>
                </div>
              )
            })}
          </div>
        )}
      </div>
    </div>
  )
}
