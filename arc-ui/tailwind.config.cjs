{
/** @type {import('tailwindcss').Config} */
module.exports = {
  content: ['./src/renderer/**/*.{html,tsx,ts}'],
  theme: {
    extend: {
      colors: {
        'arc-bg':      '#0a0a0f',
        'arc-surface': '#13131a',
        'arc-border':  '#1e1e2e',
        'arc-accent':  '#6c63ff',
        'arc-green':   '#00d67a',
        'arc-red':     '#ff4444',
        'arc-yellow':  '#ffa500',
        'arc-text':    '#e0e0e8',
        'arc-muted':   '#6b6b80',
      },
      fontFamily: {
        mono: ['JetBrains Mono', 'Fira Code', 'monospace'],
        sans: ['Inter', 'system-ui', 'sans-serif'],
      }
    }
  },
  plugins: []
}

}
