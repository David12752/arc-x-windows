import { useEffect } from 'react'
import { useEngineStore } from './store/useEngineStore'
import { CommandType } from '@shared/commands'
import { VUMeterCanvas }    from './components/VUMeterCanvas'
import { SpectrumAnalyzer } from './components/SpectrumAnalyzer'
import { PluginChainView }  from './components/PluginChainView'
import { PluginBrowser }    from './components/PluginBrowser'
import { SettingsPanel }    from './components/SettingsPanel'

export default function App(): JSX.Element {
  const setMetrics   = useEngineStore((s) => s.setMetrics)
  const handleEvent  = useEngineStore((s) => s.handleEvent)
  const activePanel  = useEngineStore((s) => s.activePanel)
  const setPanel     = useEngineStore((s) => s.setPanel)
  const masterBypassed    = useEngineStore((s) => s.masterBypassed)
  const setMasterBypass   = useEngineStore((s) => s.setMasterBypass)
  const engineConnected   = useEngineStore((s) => s.engineConnected)

  // Wire IPC → store on mount
  useEffect(() => {
    const unsubMetrics = window.arcAPI.onMetrics(setMetrics)
    const unsubEvents  = window.arcAPI.onEngineEvent(handleEvent)

    // Request initial status
    window.arcAPI.sendCommand({ type: CommandType.RequestStatus })

    return () => {
      unsubMetrics()
      unsubEvents()
    }
  }, [setMetrics, handleEvent])

  return (
    <div className="flex flex-col h-full bg-arc-bg text-arc-text select-none">
      {/* ── Title bar ─────────────────────────────────────────────────────── */}
      <header className="titlebar-drag flex items-center h-9 px-4 bg-arc-surface border-b border-arc-border shrink-0">
        <span className="text-xs font-mono font-semibold text-arc-accent mr-4 titlebar-no-drag">
          ARC
        </span>
        <span className="text-xs text-arc-muted flex-1">System Audio Host</span>

        {/* Engine status badge */}
        <span className={`titlebar-no-drag text-xs px-2 py-0.5 rounded-full mr-3 font-mono ${
          engineConnected ? 'bg-arc-green/20 text-arc-green' : 'bg-arc-red/20 text-arc-red'
        }`}>
          {engineConnected ? '● LIVE' : '○ OFFLINE'}
        </span>

        {/* Bypass toggle */}
        <button
          className={`titlebar-no-drag text-xs px-3 py-1 rounded border transition-colors ${
            masterBypassed
              ? 'bg-arc-yellow/20 border-arc-yellow text-arc-yellow'
              : 'bg-transparent border-arc-border text-arc-muted hover:border-arc-accent hover:text-arc-accent'
          }`}
          onClick={() => setMasterBypass(!masterBypassed)}
        >
          BYPASS
        </button>
      </header>

      {/* ── Main content ──────────────────────────────────────────────────── */}
      <div className="flex flex-1 overflow-hidden">

        {/* Left sidebar — metering ---------------------------------------- */}
        <aside className="flex flex-col gap-3 p-3 w-48 shrink-0 border-r border-arc-border">
          <VUMeterCanvas />
          <SpectrumAnalyzer />
        </aside>

        {/* Center — main panel -------------------------------------------- */}
        <main className="flex-1 overflow-hidden flex flex-col">
          {/* Tab bar */}
          <div className="flex border-b border-arc-border px-2 pt-2 gap-1 shrink-0">
            {(['chain', 'browser', 'settings'] as const).map((panel) => (
              <button
                key={panel}
                onClick={() => setPanel(panel)}
                className={`px-4 py-1.5 text-xs rounded-t capitalize transition-colors ${
                  activePanel === panel
                    ? 'bg-arc-surface text-arc-accent border-b-2 border-arc-accent -mb-px'
                    : 'text-arc-muted hover:text-arc-text'
                }`}
              >
                {panel}
              </button>
            ))}
          </div>

          {/* Panel content */}
          <div className="flex-1 overflow-hidden">
            {activePanel === 'chain'    && <PluginChainView />}
            {activePanel === 'browser'  && <PluginBrowser />}
            {activePanel === 'settings' && <SettingsPanel />}
          </div>
        </main>
      </div>
    </div>
  )
}
