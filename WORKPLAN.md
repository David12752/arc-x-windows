# ARC System Audio Host — Full Project Work Plan
**Version:** 2.0 | **Date:** May 2026 | **Architect:** Lead Audio Software Architect

---

## Project Vision

Refactor the monolithic JUCE application into a two-process, production-grade audio system:

| Process | Technology | Role |
|---|---|---|
| `arc-engine.exe` | C++20, JUCE Headless, ASIO | Real-time audio processing, VST3 hosting |
| `arc-ui` | Electron 30 + React 18 | Modern responsive front-end |

**IPC**: Named Pipe (commands) + Shared Memory (metrics/metering @ 60Hz)

---

## Current Status

### ✅ Phase 0 — Audio Correctness Fixes (COMPLETE)

All four critical audio defects from the initial code review have been resolved:

| Defect | File | Fix Applied |
|---|---|---|
| Look-ahead Limiter was not actually look-ahead | `AudioProcessor.h/cpp` | 3-pass ring buffer: write → forward scan → apply to delayed output |
| Volume smoothing caused L/R stereo image drift | `VSTChain.cpp`, `AudioProcessor.cpp`, `VSTPlugin.cpp` | `getNextValue()` moved to sample-outer / channel-inner loop; all misplaced `reset()` calls removed |
| PDC not implemented | `PDCEngine.h/cpp` (NEW), `VSTChain.h/cpp` | Per-slot delay lines, atomic ping-pong descriptors, wired into `updateLatency()` |
| No SRC — pitch error on rate mismatch | `SRCEngine.h/cpp` (NEW), `WASAPILoopback.h/cpp` | CatmullRom interpolator per channel, auto-passthrough when rates match |

**New files added in Phase 0:**
- `src/AudioEngine/PDCEngine.h/cpp`
- `src/AudioEngine/SRCEngine.h/cpp`

---

## Phase 1 — Engine Foundation
**Target:** Weeks 1–3 | **Status:** ✅ COMPLETE

**Goal:** Extract all audio logic into a standalone headless process (`arc-engine.exe`) with a clean public API. The existing JUCE UI remains functional during this phase.

### Tasks

#### 1.1 Project Restructure ✅ (defined)
New directory layout:
```
src/
  Engine/
    EngineCore.h/cpp          ← Top-level controller (replaces Application for engine)
    AudioPipeline.h/cpp       ← RT chain: SRC→VSTChain→PDC→Limiter
  Concurrency/
    SPSCQueue.h               ← Header-only SPSC lock-free queue
    MetricsFrame.h            ← POD metrics struct (lives in SHM)
  IPC/
    EngineCommand.h           ← std::variant command types
    CommandServer.h/cpp       ← Named Pipe server (Non-RT)
    MetricsPublisher.h/cpp    ← SHM seqlock writer (Non-RT, 60Hz)
  engine_main.cpp             ← Headless JUCEApplication entry point
```

#### 1.2 Concurrency Layer
- [x] `SPSCQueue<T, Capacity>` — wait-free enqueue/dequeue, cache-line aligned, power-of-2 ring
- [x] `MetricsFrame` — POD struct with seqlock header for zero-copy SHM reads

#### 1.3 IPC Layer
- [x] `EngineCommand` — `std::variant<SetVolumeCmd, SetBypassCmd, LoadPluginCmd, ...>`
- [x] `CommandServer` — Windows Named Pipe `\\.\pipe\arc-command`, reads framed binary commands, enqueues to SPSC
- [x] `MetricsPublisher` — `CreateFileMapping("arc_metrics_shm")`, seqlock write pattern, pumped at 60Hz

#### 1.4 Engine Layer
- [x] `AudioPipeline` — RT-safe wrapper around `VSTChain` + `AudioProcessor`; drains SPSC command queue per block
- [x] `EngineCore` — owns all subsystems, sets up `AudioDeviceManager` callback to `AudioPipeline`, starts IPC threads
- [x] `engine_main.cpp` — headless `JUCEApplication` subclass; single-instance lock

#### 1.5 Build System
- [x] Add `arc-engine` console app target to `CMakeLists.txt` (no `juce_gui_basics`, no `juce_opengl`)
- [x] Keep original `ARCSystemAudioHost` target unchanged (builds in parallel)

### Deliverable
Running `arc-engine.exe` from CLI that:
- Initializes WASAPI loopback + output device
- Accepts commands on `\\.\pipe\arc-command`
- Writes metrics to `arc_metrics_shm` every 16ms
- Can be tested with a simple pipe writer tool

---

## Phase 2 — ASIO Backend & Real-Time Threading
**Target:** Weeks 4–6 | **Status:** 🔄 IN PROGRESS

**Goal:** Replace WASAPI output with ASIO for deterministic low-latency. Enforce strict no-mutex policy in the RT audio path.

### Tasks

#### 2.1 ASIO SDK Integration
- [ ] Download Steinberg ASIO SDK 2.3 (free, non-GPL)
- [x] Add `ASIO_SDK_PATH` CMake option (`ARC_JUCE_ASIO_VALUE` conditional, both targets)
- [x] Set `JUCE_ASIO=${ARC_JUCE_ASIO_VALUE}`, include `asiosdk/common/` when set
- [ ] In `EngineCore`, force `"ASIO"` device type in `juce::AudioDeviceManager`
- [ ] Add ASIO device enumeration to `CommandServer` response for UI discovery

#### 2.2 No-Mutex RT Policy ✅ COMPLETE
- [x] Remove `callbackMutex` from `AudioDeviceManager::audioDeviceIOCallbackWithContext`
- [x] Replace with `std::atomic<AudioCallback*>` with `acquire` load in RT callback
- [x] Fix RT heap alloc: pre-allocate `processingBuffer` in `audioDeviceAboutToStart`, silence+xrun if mismatch in callback
- [x] Remove try/catch + Logger::error from RT callback
- [x] Implement `SlotSnapshot` atomic ping-pong in `VSTChain`
  - Non-RT: writes into staging slot under `chainMutex` write lock, then `activeSnapshot_.store(release)`
  - RT: single `atomic::load(acquire)` — zero lock, zero heap
- [x] `rebuildSnapshot()` wired at all 7 VSTChain mutation sites
- [x] Audit entire RT path: no heap, no lock, no syscall

#### 2.3 RT-Guard Asserts ✅ COMPLETE
- [x] `src/Utils/RTGuard.h` — `ARC_ASSERT_RT_SAFE()` / `ARC_ASSERT_NONRT_SAFE()` macros
- [x] `thread_local bool isAudioCallbackThread_` set in `audioDeviceIOCallbackWithContext`
- [x] Guard wired into `AudioPipeline::processBlock` and `VSTChain::processBlock`

#### 2.4 Loopback-to-Output Ring Buffer ✅ COMPLETE
- [x] `src/Concurrency/AudioBlock.h` — trivially copyable, fixed-size, `fill()` / `monoToStereo()` / `silence()` helpers
- [x] `SPSCQueue<AudioBlock, 16> loopbackQueue_` owned by `EngineCore`
- [x] `WASAPILoopback::setLoopbackQueue()` — queue push path in capture thread (replaces mutex+callback)
- [x] `AudioPipeline::setLoopbackQueue()` — pulls from queue at top of `processBlock`; silence+xrun on empty
- [x] `EngineCore::initialize()` wires both ends before `loopback_.start()`
- [x] `EngineCore::shutdown()` detaches queue before `loopback_.stop()`

#### 2.5 MMCSS Thread Priority
- [ ] Verify `AvSetMmThreadCharacteristicsA("Pro Audio", ...)` is called on output callback thread
- [ ] Add thread-priority monitoring to `CPUMonitor`

### Deliverable
RT path: **zero mutex, zero heap, zero syscalls** in `AudioPipeline::processBlock` and `VSTChain::processBlock`.
Loopback queue provides deterministic decoupling between WASAPI capture jitter and output callback timing.
ASIO build optional — enable by setting `ASIO_SDK_PATH` at CMake configure time.

---

## Phase 3 — Electron Bridge & UI
**Target:** Weeks 7–10 | **Status:** 🔄 IN PROGRESS

**Goal:** Build the Electron/React front-end that communicates with `arc-engine.exe` via SHM + Named Pipes.

### Tasks

#### 3.1 Electron Project Setup ✅ COMPLETE
- [x] Created `arc-ui/` with `electron-vite` scaffolding
- [x] `package.json` — electron 31, react 18, tailwind, @tanstack/react-virtual, zustand, node-addon-api
- [x] `electron.vite.config.ts` — COOP/COEP headers for SharedArrayBuffer, path aliases
- [x] `tailwind.config.cjs` — ARC dark color palette (arc-bg, arc-surface, arc-accent, etc.)
- [x] `tsconfig.json` + `tsconfig.node.json`

#### 3.2 Native Addon (N-API) ✅ COMPLETE
- [x] `arc-ui/native/shm_reader.cpp` — N-API addon
  - `OpenFileMappingA("arc_metrics_shm")` → `MapViewOfFile`
  - Seqlock-consistent `readFrame()` (max 16 retries, `_mm_pause()` spin)
  - Returns fully typed JS object (not SAB — simpler, no COOP needed for this path)
  - Lazy `open()` on first `readFrame()` call if engine starts after UI
- [x] `arc-ui/native/binding.gyp` — MSVC/C++17 build config

#### 3.3 Shared Types ✅ COMPLETE
- [x] `src/shared/MetricsFrame.ts` — TypeScript mirror of C++ MetricsFrame struct + `linearToDb()` / `dbToMeterPos()` helpers
- [x] `src/shared/commands.ts` — `CommandType` enum + `EngineCommand` discriminated union + `EngineEvent` union + `ScannedPlugin`
- [x] `src/shared/protocol.ts` — `encodeCommand()` binary framing encoder (matches `CommandServer.cpp` wire format)

#### 3.4 IPC Transport ✅ COMPLETE
- [x] `src/main/ipc/CommandClient.ts` — lazy-connect `net.Socket` to `\\.\pipe\arc-command`, auto-reconnect with exponential backoff, pending queue during reconnect
- [x] `src/main/ipc/MetricsReader.ts` — 60Hz setInterval polling via native addon, dedup by sequence number, broadcasts via `webContents.send('metrics:frame', ...)`
- [x] `src/main/ipc/EventListener.ts` — reads newline-delimited JSON from `\\.\pipe\arc-events`, dispatches to renderer + optional main-process callback
- [x] `src/main/tray.ts` — system tray with bypass toggle, show/hide, quit

#### 3.5 Main Process ✅ COMPLETE
- [x] `src/main/index.ts` — single-instance lock, COOP/COEP header injection via `session.defaultSession.webRequest`, IPC handler setup, tray creation, clean shutdown

#### 3.6 Preload Bridge ✅ COMPLETE
- [x] `src/preload/index.ts` — `contextBridge.exposeInMainWorld('arcAPI', ...)` with `sendCommand()`, `onMetrics()`, `onEngineEvent()` (all return cleanup functions)
- [x] `src/preload/index.d.ts` — `Window.arcAPI` type augmentation

#### 3.7 React UI ✅ COMPLETE
- [x] `src/renderer/store/useEngineStore.ts` — Zustand store (metrics, engine state, plugin slots, browser, UI panel) with all action methods
- [x] `src/renderer/App.tsx` — titlebar + bypass toggle + panel routing (Chain / Browser / Settings)
- [x] `src/renderer/components/VUMeterCanvas.tsx` — Canvas 2D stereo peak/RMS/hold meter with 3-zone gradient
- [x] `src/renderer/components/SpectrumAnalyzer.tsx` — 64-bin log-scale spectrum with per-bin peak-hold decay
- [x] `src/renderer/components/PluginChainView.tsx` — 10-slot chain, drag-reorder (HTML5 DnD), bypass toggle, mix slider, CPU per slot, xrun display
- [x] `src/renderer/components/PluginBrowser.tsx` — `@tanstack/react-virtual` list, live search, category filters, scan path input, load-to-slot
- [x] `src/renderer/components/SettingsPanel.tsx` — device/SR/buffer controls, input/output gain sliders, full diagnostics grid

#### 3.8 IPC Protocol Upgrade (FlatBuffers) — DEFERRED to Phase 3.5
- [ ] Define `schema/arc_commands.fbs`
- [ ] Generate C++ + TypeScript bindings
- [ ] Replace Phase 1 binary protocol in `CommandServer` + `CommandClient`

### Deliverable
Full UI running against live `arc-engine.exe`. Plugin chain visible and interactive. Metering updating at 60fps via SHM. Tray integration for background operation.

**To run in dev:**
```bash
cd arc-ui
npm install
npm run rebuild   # builds shm_reader.node
npm run dev       # starts Electron + Vite HMR
```

---

## Phase 4 — Virtual Audio Device
**Target:** Months 2–4 | **Status:** 🔄 IN PROGRESS

**Goal:** Route system audio transparently through ARC Engine for real-time VST processing.

### Tier 1 — VB-Cable Wrapper (Month 2, 1 week) ✅ COMPLETE
- [x] `src/AudioEngine/VBCableWrapper.h` / `.cpp` — enumerate WASAPI render+capture devices, detect "CABLE Input" / "CABLE Output" by name
- [x] `VBCableWrapper::detectVBCable(AudioDeviceManager&)` — scans both input and output device lists
- [x] `VBCableWrapper::applyRouting()` / `revertToDefault()` — set CABLE Input as engine output device
- [x] `EngineCore::initialize()` — auto-detect VB-Cable on startup, cache `VBCableInfo`
- [x] `EngineCore::broadcastVBCableStatus()` — push `vbcable_status` JSON event on connect and on rescan
- [x] `EngineCommand.h` — added `DetectVBCable = 0x11`, `SetVBCableRouting = 0x12`; added `DetectVBCableCmd`, `SetVBCableRoutingCmd` to variant
- [x] `EngineCore::handleNonRTCommand()` — handles `DetectVBCableCmd` (rescan) and `SetVBCableRoutingCmd` (apply/revert)
- [x] `CMakeLists.txt` — registered `VBCableWrapper.cpp` / `.h` in `AUDIO_ENGINE_SOURCES` / `AUDIO_ENGINE_HEADERS`
- [x] `arc-ui/src/shared/commands.ts` — added `DetectVBCable`, `SetVBCableRouting` to `CommandType` enum; added `vbcable_status`, `routing_applied`, `error` to `EngineEvent` union
- [x] `arc-ui/src/shared/protocol.ts` — added binary encoders for `DetectVBCable` (no payload) and `SetVBCableRouting` (uint8 enable)
- [x] `arc-ui/src/renderer/store/useEngineStore.ts` — added `vbcableInstalled`, `vbcableRenderDevice`, `vbcableCaptureDevice`, `vbcableRouting` state; handlers for `vbcable_status`, `routing_applied`, `error` events; `detectVBCable()` and `setVBCableRouting()` actions
- [x] `arc-ui/src/renderer/components/SettingsPanel.tsx` — VB-Cable section: status badge, routing diagram, Apply/Revert buttons, install prompt with download link

**Routing model:**
```
App Audio → [CABLE Input (render)] ← arc-engine output
                      ↓
[CABLE Output (capture)] → WASAPILoopback → AudioPipeline → Real Speakers
```

**Next steps for Tier 1:**
- [ ] Bundle `VBCABLE_Setup_x64.exe` in `resources/` for one-click install (optional convenience)
- [ ] First-launch prompt if VB-Cable not detected

### Tier 2 — WDM Kernel Driver (Months 3–4)
- [ ] Study Microsoft `msvad` sample (Windows Driver Kit)
- [ ] Implement `arc-vad.sys` — `IMiniportWaveRT` for render endpoint
- [ ] DMA cyclic buffer → kernel event → `arc-engine.exe` user-mode reader
- [ ] WHQL signing: obtain EV Code Signing Certificate (~$500/year)
- [ ] Microsoft Partner Center attestation signing
- [ ] Installer: `SetupAPI` to install `.inf` / `.cat` driver package

### Tier 3 — ASIO Virtual Device (Month 5+)
- [ ] Expose `"ARC Processing OUT"` ASIO device — DAW sends master bus here
- [ ] Expose `"ARC Processing IN"` ASIO device — DAW reads processed signal
- [ ] Implement ASIO Host SDK user-mode protocol (no kernel code)
- [ ] Zero-copy IPC between virtual ASIO device and `arc-engine` pipeline

---

## Phase 5 — Testing & Production
**Target:** Month 5–6 | **Status:** ⏳ NOT STARTED

### Testing
- [ ] Unit tests for `SPSCQueue`, `PDCEngine`, `SRCEngine`, `LookAheadLimiter`
- [ ] Integration test: loopback → pipeline → output with known sine-wave input
- [ ] Latency measurement test: verify PDC compensates correctly
- [ ] SRC quality test: 44.1→48kHz sine sweep — measure THD+N
- [ ] RT-safety test: detect any mutex/heap/syscall in audio callback (custom tooling)
- [ ] Stress test: 10 plugins, 64-sample buffer, 24h continuous run

### CI/CD
- [ ] GitHub Actions: Windows Server 2022, MSVC 2022
- [ ] Build matrix: Debug/Release × 44100/48000/96000
- [ ] CTest integration
- [ ] Automated installer build (NSIS or WiX)

### Production Polish
- [ ] Error reporting / crash dump (WER / Breakpad)
- [ ] Auto-update (Electron `autoUpdater` + Squirrel)
- [ ] Telemetry opt-in (CPU, sample rate, plugin count — no personal data)
- [ ] Documentation: user manual + developer API docs (Doxygen)

---

## Stack Summary

### C++ Engine
| Category | Choice | Notes |
|---|---|---|
| Lock-free SPSC | Custom `SPSCQueue<T,N>` → later: `cameron314/readerwriterqueue` | Header-only, zero deps for Phase 1 |
| IPC Serialization | Binary framing (Phase 1) → FlatBuffers 23.x (Phase 3) | Zero-copy on read side |
| Shared Memory | Win32 `CreateFileMapping` / `MapViewOfFile` | No extra deps |
| SRC | `juce::CatmullRomInterpolator` (RT) | Already in JUCE DSP |
| ASIO | Steinberg ASIO SDK 2.3 (Phase 2) | Free, non-GPL |
| Testing | Catch2 v3 + FakeIt | Already partially set up |

### Electron/React UI
| Category | Choice | Notes |
|---|---|---|
| State management | Zustand | Minimal re-renders, slice subscriptions |
| Canvas rendering | `OffscreenCanvas` + Web Worker | Zero main-thread jank for meters |
| Animations | React Spring v9 | Physics-based drag-reorder |
| Styling | Tailwind CSS + CSS Variables | Dynamic theming without JS re-renders |
| Build | Vite + `electron-vite` | Sub-second HMR |
| Native addons | N-API (`node-addon-api`) | ABI-stable across Electron versions |
| Plugin list | `@tanstack/virtual` | Virtualizes 1000+ items with no lag |

---

## File Change Log

| File | Status | Phase | Description |
|---|---|---|---|
| `src/AudioEngine/PDCEngine.h/cpp` | ✅ NEW | 0 | Plugin Delay Compensation engine |
| `src/AudioEngine/SRCEngine.h/cpp` | ✅ NEW | 0 | Sample Rate Conversion engine |
| `src/AudioEngine/AudioProcessor.h/cpp` | ✅ MODIFIED | 0 | True look-ahead limiter; gain smoothing fix |
| `src/VST/VSTChain.h/cpp` | ✅ MODIFIED | 0 | Volume smoothing fix; PDC wiring |
| `src/VST/VSTPlugin.h/cpp` | ✅ MODIFIED | 0 | Gain/mix smoothing fix; dry PDC delay line |
| `src/AudioEngine/WASAPILoopback.h/cpp` | ✅ MODIFIED | 0 | SRC integration; de-interleave fix |
| `CMakeLists.txt` | ✅ MODIFIED | 0 | PDCEngine + SRCEngine registered |
| `src/Concurrency/SPSCQueue.h` | ✅ NEW | 1 | Lock-free SPSC queue |
| `src/Concurrency/MetricsFrame.h` | ✅ NEW | 1 | SHM metrics POD struct |
| `src/Concurrency/AudioBlock.h` | ✅ NEW | 2 | Fixed-size SPSC-safe audio transfer unit |
| `src/IPC/EngineCommand.h` | ✅ NEW | 1 | Command variant types |
| `src/IPC/CommandServer.h/cpp` | ✅ NEW | 1 | Named Pipe server |
| `src/IPC/MetricsPublisher.h/cpp` | ✅ NEW | 1 | SHM seqlock writer |
| `src/Engine/AudioPipeline.h/cpp` | ✅ NEW | 1 | RT pipeline wrapper |
| `src/Engine/EngineCore.h/cpp` | ✅ NEW | 1 | Headless engine controller |
| `src/engine_main.cpp` | ✅ NEW | 1 | Headless JUCE entry point |
| `src/Utils/RTGuard.h/cpp` | ✅ NEW | 2 | RT-thread safety assertions |
| `arc-ui/package.json` | ✅ NEW | 3 | Electron UI project manifest |
| `arc-ui/electron.vite.config.ts` | ✅ NEW | 3 | Vite + COOP/COEP config |
| `arc-ui/native/shm_reader.cpp` | ✅ NEW | 3 | N-API SHM seqlock reader addon |
| `arc-ui/native/binding.gyp` | ✅ NEW | 3 | node-gyp build definition |
| `arc-ui/src/shared/MetricsFrame.ts` | ✅ NEW | 3 | TS mirror of MetricsFrame POD |
| `arc-ui/src/shared/commands.ts` | ✅ NEW | 3 | EngineCommand + EngineEvent types |
| `arc-ui/src/shared/protocol.ts` | ✅ NEW | 3 | Binary command encoder |
| `arc-ui/src/main/index.ts` | ✅ NEW | 3 | Electron main process |
| `arc-ui/src/main/ipc/CommandClient.ts` | ✅ NEW | 3 | Named pipe command sender |
| `arc-ui/src/main/ipc/MetricsReader.ts` | ✅ NEW | 3 | 60Hz SHM metrics poller |
| `arc-ui/src/main/ipc/EventListener.ts` | ✅ NEW | 3 | Engine event pipe reader |
| `arc-ui/src/main/tray.ts` | ✅ NEW | 3 | System tray integration |
| `arc-ui/src/preload/index.ts` | ✅ NEW | 3 | contextBridge API surface |
| `arc-ui/src/renderer/store/useEngineStore.ts` | ✅ NEW | 3 | Zustand engine state store |
| `arc-ui/src/renderer/App.tsx` | ✅ NEW | 3 | Root layout + routing |
| `arc-ui/src/renderer/components/VUMeterCanvas.tsx` | ✅ NEW | 3 | Stereo level meter |
| `arc-ui/src/renderer/components/SpectrumAnalyzer.tsx` | ✅ NEW | 3 | 64-bin spectrum display |
| `arc-ui/src/renderer/components/PluginChainView.tsx` | ✅ NEW | 3 | 10-slot drag-reorder chain |
| `arc-ui/src/renderer/components/PluginBrowser.tsx` | ✅ NEW | 3 | Virtualized plugin browser |
| `arc-ui/src/renderer/components/SettingsPanel.tsx` | ✅ NEW | 3 | Device + diagnostics panel |
