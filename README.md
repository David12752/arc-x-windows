# ARC System Audio Host

A professional Windows desktop application for system-wide VST3 plugin hosting, specifically designed to route all system audio output through VST3 effects before sending to speakers.

## Features

### Audio Routing
- **System-wide audio capture** via WASAPI loopback
- **Low latency processing** (<10ms roundtrip target)
- **Multiple sample rates**: 44.1kHz, 48kHz, 88.2kHz, 96kHz
- **Configurable buffer sizes**: 64, 128, 256, 512, 1024 samples
- **Look-ahead limiter** on master output for safety

### VST3 Plugin Hosting
- **Full VST3 plugin support** using JUCE framework
- **Plugin scanner** with background scanning
- **Plugin chain** supporting up to 10 plugins in series
- **Drag-and-drop reordering** of plugin chain
- **Per-plugin bypass** and CPU monitoring
- **Plugin state saving/loading** (presets)
- **Plugin crash recovery** (blacklist problematic plugins)

### User Interface
- **Modern dark theme** with customizable accent colors
- **Real-time VU meters** with peak hold
- **Spectrum analyzer** with multiple display modes
- **Plugin browser** with search and favorites
- **System tray integration** for quick access

### IK Multimedia ARC Support
- **Automatic ARC detection**
- **A/B comparison mode**
- **Quick preset switching**

## Building

### Prerequisites

1. **CMake** 3.22 or higher
2. **Visual Studio 2022** with C++ development tools
3. **JUCE Framework** 7.x
4. **VST3 SDK** (optional, for additional VST3 hosting features)

### Build Steps

```bash
# Clone the repository
git clone https://github.com/your-repo/arc-system-audio-host.git
cd arc-system-audio-host

# Create build directory
mkdir build
cd build

# Configure with CMake
cmake .. -G "Visual Studio 17 2022" -A x64 -DJUCE_PATH="C:/path/to/JUCE"

# Build
cmake --build . --config Release

# Or open the Visual Studio solution
start ARCSystemAudioHost.sln
```

### CMake Options

| Option | Description | Default |
|--------|-------------|---------|
| `JUCE_PATH` | Path to JUCE framework | Auto-download |
| `VST3_SDK_PATH` | Path to VST3 SDK | Not set |
| `CMAKE_BUILD_TYPE` | Build type (Debug/Release) | Release |

## Project Structure

```
src/
├── main.cpp                    # Application entry point
├── Application.h/cpp           # Main application class
├── AudioEngine/
│   ├── AudioDeviceManager.h/cpp  # Audio device management
│   ├── AudioProcessor.h/cpp      # Core audio processing
│   ├── PluginHost.h/cpp          # VST3 plugin hosting
│   ├── WASAPILoopback.h/cpp      # System audio capture
│   └── VirtualAudioDevice.h/cpp  # Virtual audio routing
├── VST/
│   ├── VSTScanner.h/cpp          # Plugin scanner
│   ├── VSTPlugin.h/cpp           # Plugin wrapper
│   ├── VSTChain.h/cpp            # Plugin chain management
│   └── PluginWindow.h/cpp        # Plugin editor windows
├── UI/
│   ├── MainWindow.h/cpp          # Main application window
│   ├── PluginBrowser.h/cpp       # Plugin browser panel
│   ├── PluginChainView.h/cpp     # Chain visualization
│   ├── SettingsDialog.h/cpp      # Settings UI
│   ├── VUMeter.h/cpp             # Audio level meters
│   ├── SpectrumAnalyzer.h/cpp    # Frequency display
│   ├── PluginSlot.h/cpp          # Individual slot UI
│   └── LookAndFeel.h/cpp         # Custom theming
└── Utils/
    ├── PresetManager.h/cpp       # Preset management
    ├── ConfigManager.h/cpp       # Configuration storage
    ├── Logger.h/cpp              # Logging system
    └── CPUMonitor.h/cpp          # CPU usage tracking
```

## Usage

### First Run

1. Launch the application
2. Select your audio output device in the toolbar
3. Configure sample rate and buffer size for your needs
4. The plugin scanner will automatically find installed VST3 plugins

### Adding Plugins

1. Find plugins in the left panel browser
2. Double-click or drag to add to the chain
3. Plugins process audio in order from top to bottom

### Plugin Controls

- **B** button: Bypass plugin
- **E** button: Open plugin editor
- **X** button: Remove from chain
- Drag plugins to reorder

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+S` | Save preset |
| `Ctrl+O` | Load preset |
| `Space` | Toggle master bypass |
| `1-8` | Load quick slot preset |

## Audio Routing Options

### WASAPI Loopback (Default)
Captures all system audio without additional drivers. Minimal latency.

### Virtual Audio Cable (Recommended for full routing)
For complete audio routing, install a virtual audio cable driver:
- VB-Audio Virtual Cable (free)
- VoiceMeeter (free)
- Virtual Audio Cable (paid)

Set the virtual cable as your default Windows output, then configure this application to capture from it and output to your real speakers.

## Troubleshooting

### No Audio Output
1. Check output device selection
2. Verify plugin chain isn't muted
3. Check master volume level

### High Latency
1. Reduce buffer size (256 or lower)
2. Close other audio applications
3. Use ASIO driver if available

### Plugin Not Loading
1. Check plugin blacklist in settings
2. Verify plugin is VST3 format
3. Rescan plugins

### High CPU Usage
1. Check per-plugin CPU meters
2. Bypass CPU-intensive plugins
3. Increase buffer size

## License

GNU AGPL v3 (see `LICENSE`), because the app is built with JUCE 8, whose open-source licence is AGPLv3. The VST3 SDK is
Steinberg's and comes under its own licence.

## Credits

- **JUCE Framework** - Audio application framework
- **VST3 SDK** - Steinberg plugin format

## Support

For issues and feature requests, please use the GitHub issue tracker.

