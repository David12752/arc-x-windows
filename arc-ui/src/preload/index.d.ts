import type { arcAPI } from './index'

// Augment the global Window type so TypeScript knows about window.arcAPI
declare global {
  interface Window {
    arcAPI: typeof arcAPI
  }
}
