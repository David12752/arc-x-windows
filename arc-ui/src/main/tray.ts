/**
 * System tray integration.
 * Shows a minimal tray icon with quick access to bypass, show/hide, and quit.
 */

import { app, Tray, Menu, nativeImage, BrowserWindow } from 'electron'
import path from 'path'
import type { CommandClient } from './ipc/CommandClient'
import { CommandType } from '@shared/commands'

export function createTray(getWindow: () => BrowserWindow, client: CommandClient): Tray {
  const iconPath = path.join(__dirname, '../../resources/tray-icon.png')
  let icon = nativeImage.createFromPath(iconPath)
  if (icon.isEmpty()) {
    // Fallback: programmatic 16×16 purple square (RGBA, no external file needed)
    const size = 16
    const buf  = Buffer.alloc(size * size * 4)
    for (let i = 0; i < size * size; i++) {
      buf[i * 4 + 0] = 108  // R
      buf[i * 4 + 1] = 99   // G
      buf[i * 4 + 2] = 255  // B
      buf[i * 4 + 3] = 255  // A
    }
    icon = nativeImage.createFromBuffer(buf, { width: size, height: size })
  }
  icon = icon.resize({ width: 16, height: 16 })
  const tray = new Tray(icon)

  let bypassed = false

  const buildMenu = () => Menu.buildFromTemplate([
    {
      label: 'ARC System Audio Host',
      enabled: false,
    },
    { type: 'separator' },
    {
      label: bypassed ? 'Bypass ON (click to disable)' : 'Bypass OFF (click to enable)',
      click: () => {
        bypassed = !bypassed
        client.sendCommand({ type: CommandType.SetBypass, bypass: bypassed })
        tray.setContextMenu(buildMenu())
      },
    },
    { type: 'separator' },
    {
      label: 'Show Window',
      click: () => {
        const win = getWindow()
        win.show()
        win.focus()
      },
    },
    {
      label: 'Quit',
      click: () => {
        client.sendCommand({ type: CommandType.Shutdown })
        setTimeout(() => app.quit(), 500)
      },
    },
  ])

  tray.setToolTip('ARC System Audio Host')
  tray.setContextMenu(buildMenu())

  tray.on('double-click', () => {
    const win = getWindow()
    win.isVisible() ? win.hide() : win.show()
  })

  return tray
}
