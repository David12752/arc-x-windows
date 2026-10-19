/**
 * Electron main process entry point.
 *
 * Responsibilities:
 *   1. Create the BrowserWindow with Cross-Origin-Isolation headers
 *      (required for SharedArrayBuffer in renderer)
 *   2. Launch arc-engine.exe as a child process (if not already running)
 *   3. Start MetricsReader (60Hz SHM polling)
 *   4. Start CommandClient (Named Pipe sender)
 *   5. Start EventListener (Named Pipe receiver)
 *   6. Set up IPC handlers so renderer can send commands and receive metrics
 *   7. Create the system tray
 */

import { app, BrowserWindow, ipcMain, protocol, session, shell } from 'electron'
import { join } from 'path'
import { is } from '@electron-toolkit/utils'
import { CommandClient }  from './ipc/CommandClient'
import { MetricsReader }  from './ipc/MetricsReader'
import { EventListener }  from './ipc/EventListener'
import { createTray }     from './tray'
import type { EngineCommand } from '@shared/commands'

// ---------------------------------------------------------------------------
// Single-instance lock
// ---------------------------------------------------------------------------
if (!app.requestSingleInstanceLock()) {
  app.quit()
  process.exit(0)
}

// ---------------------------------------------------------------------------
// Module-level singletons
// ---------------------------------------------------------------------------
const commandClient  = new CommandClient()
const metricsReader  = new MetricsReader()
const eventListener  = new EventListener()

let mainWindow: BrowserWindow | null = null
let isQuitting = false

function showMainWindow(): BrowserWindow {
  if (!mainWindow || mainWindow.isDestroyed())
    mainWindow = createWindow()

  if (mainWindow.isMinimized())
    mainWindow.restore()

  mainWindow.show()
  mainWindow.focus()
  return mainWindow
}

// ---------------------------------------------------------------------------
// Window creation
// ---------------------------------------------------------------------------
function createWindow(): BrowserWindow {
  const win = new BrowserWindow({
    width:           1280,
    height:          800,
    minWidth:        900,
    minHeight:       600,
    backgroundColor: '#0a0a0f',
    titleBarStyle:   'hidden',
    webPreferences: {
      preload:             join(__dirname, '../preload/index.js'),
      sandbox:             false,
      contextIsolation:    true,
      nodeIntegration:     false,
      webSecurity:         true,
    },
  })

  // Load the renderer
  if (is.dev && process.env['ELECTRON_RENDERER_URL']) {
    win.loadURL(process.env['ELECTRON_RENDERER_URL'])
    win.webContents.openDevTools()
  } else {
    win.loadFile(join(__dirname, '../renderer/index.html'))
  }

  // Open all external (http/https) links in the default system browser
  // instead of trying to open a new Electron window.
  win.webContents.setWindowOpenHandler(({ url }) => {
    if (url.startsWith('https://') || url.startsWith('http://')) {
      shell.openExternal(url)
    }
    return { action: 'deny' }
  })

  // The app is designed to keep running in the tray on Windows. Closing the
  // window should therefore hide it instead of destroying the BrowserWindow.
  win.on('close', (event) => {
    if (!isQuitting) {
      event.preventDefault()
      win.hide()
    }
  })

  // Clear the module-level reference when the window is destroyed so that
  // app.on('second-instance') does not call .show()/.focus() on a stale
  // (destroyed) BrowserWindow and throw "Object has been destroyed".
  win.on('closed', () => {
    if (mainWindow === win) mainWindow = null
  })

  return win
}

// ---------------------------------------------------------------------------
// Cross-Origin-Isolation headers (required for SharedArrayBuffer).
// In production, intercept all requests from the renderer and inject headers.
// In dev, Vite's dev server already sets these (see electron.vite.config.ts).
// ---------------------------------------------------------------------------
function setupCOIHeaders(): void {
  session.defaultSession.webRequest.onHeadersReceived((details, callback) => {
    callback({
      responseHeaders: {
        ...details.responseHeaders,
        'Cross-Origin-Opener-Policy':   ['same-origin'],
        'Cross-Origin-Embedder-Policy': ['require-corp'],
      },
    })
  })
}

// ---------------------------------------------------------------------------
// IPC handlers
// ---------------------------------------------------------------------------
function setupIPC(): void {
  // Renderer → engine: send a command
  ipcMain.on('engine:sendCommand', (_event, cmd: EngineCommand) => {
    commandClient.sendCommand(cmd)
  })

  // Renderer asks for current connection status
  ipcMain.handle('engine:status', () => ({
    connected: true, // CommandClient auto-reconnects; always "trying"
  }))
}

// ---------------------------------------------------------------------------
// App lifecycle
// ---------------------------------------------------------------------------
app.whenReady().then(() => {
  setupCOIHeaders()
  setupIPC()

  mainWindow = createWindow()
  const win = mainWindow

  // Start IPC subsystems
  commandClient.start()
  metricsReader.start(win)
  eventListener.start(win, (event) => {
    // Main-process side effects for engine events (e.g. tray tooltip update)
    if (event.event === 'xrun')
      console.warn(`[ARC] xrun count: ${event.count}`)
  })

  createTray(() => showMainWindow(), commandClient)

  app.on('activate', () => {
    showMainWindow()
  })
})

app.on('second-instance', () => {
  showMainWindow()
})

app.on('window-all-closed', () => {
  // Keep running in tray on Windows
  // app.quit() is called from the tray "Quit" menu item
})

app.on('before-quit', () => {
  isQuitting = true
  metricsReader.stop()
  eventListener.stop()
  commandClient.stop()
})

// Prevent navigation away from app (security).
// NOTE: setWindowOpenHandler is intentionally NOT set here — the main window
// has its own handler (shell.openExternal for http/https links). Setting it
// here would override that and block the VB-Cable download link.
app.on('web-contents-created', (_event, contents) => {
  contents.on('will-navigate', (event) => event.preventDefault())
})
