/*
 * ============================================================================
 * ARC System Audio Host - SHM Reader Native Addon
 *
 * N-API (node-addon-api) addon for Windows Shared Memory access.
 * Implements seqlock-consistent reads of MetricsFrame from arc_metrics_shm.
 *
 * Exposed JS API:
 *   open()          → void   — opens/maps the SHM (no-op if already open)
 *   close()         → void   — unmaps and closes the SHM handle
 *   isOpen()        → bool   — true if mapping is active
 *   readFrame()     → MetricsFrameJS | null
 *                     Performs seqlock-consistent read. Returns null if engine
 *                     not running (magic mismatch or stale sequence).
 *
 * MetricsFrameJS layout matches MetricsFrame POD exactly (see MetricsFrame.h).
 * All numeric fields returned as JS numbers (float64 representation of float32).
 * spectrum[] returned as Float32Array (zero-copy view over a Node Buffer).
 *
 * THREAD SAFETY
 * -------------
 * This addon is called from the Electron main process on a single thread
 * (the metrics polling setInterval). It is NOT safe to call from multiple
 * threads simultaneously without external synchronisation.
 * ============================================================================
 */

#include <napi.h>
#include <windows.h>
#include <cstring>
#include <cstdint>
#include <atomic>

// ---------------------------------------------------------------------------
// Mirror of src/Concurrency/MetricsFrame.h
// IMPORTANT: keep in sync with C++ side on any struct change.
// ---------------------------------------------------------------------------
constexpr std::size_t ARC_SHM_SIZE        = 65536;
constexpr uint32_t    ARC_SHM_MAGIC       = 0x41524332; // "ARC2"
constexpr int         METRICS_SPECTRUM_BINS = 64;
constexpr const char* ARC_SHM_NAME        = "arc_metrics_shm";
constexpr int         MAX_PLUGIN_SLOTS    = 10;

#pragma pack(push, 1)
struct SHMHeader
{
    uint64_t sequence;
    uint32_t magic;
    uint32_t frameSize;
};

struct MetricsFrame
{
    uint64_t sequenceNumber;
    uint64_t timestampMs;

    float peakL, peakR;
    float rmsL, rmsR;
    float peakHoldL, peakHoldR;

    float limiterGainReductionDb;

    float cpuPercent;
    float cpuPeakPercent;

    int32_t totalLatencySamples;
    float   totalLatencyMs;
    int32_t xrunCount;

    uint8_t engineRunning;
    uint8_t masterBypassed;
    uint8_t srcActive;
    uint8_t reserved0;

    int32_t activePluginCount;
    float   slotCpuPercent[10];
    uint8_t slotBypassed[10];
    uint8_t pad0[2];

    double  engineSampleRate;
    double  loopbackSampleRate;
    int32_t bufferSize;
    uint8_t pad1[4];

    float spectrum[METRICS_SPECTRUM_BINS];
};
#pragma pack(pop)

// ---------------------------------------------------------------------------
// Module-level SHM state
// ---------------------------------------------------------------------------
static HANDLE  g_hMapping  = nullptr;
static void*   g_pView     = nullptr;

static inline SHMHeader*   header()    { return reinterpret_cast<SHMHeader*>(g_pView); }
static inline MetricsFrame* frame()    { return reinterpret_cast<MetricsFrame*>(
                                            static_cast<uint8_t*>(g_pView) + sizeof(SHMHeader)); }

// ---------------------------------------------------------------------------
// open()
// ---------------------------------------------------------------------------
static Napi::Value Open(const Napi::CallbackInfo& info)
{
    auto env = info.Env();

    if (g_pView != nullptr)
        return env.Undefined(); // already open

    g_hMapping = OpenFileMappingA(FILE_MAP_READ, FALSE, ARC_SHM_NAME);
    if (g_hMapping == nullptr)
    {
        // Engine not running yet — this is expected on startup.
        return env.Undefined();
    }

    g_pView = MapViewOfFile(g_hMapping, FILE_MAP_READ, 0, 0, ARC_SHM_SIZE);
    if (g_pView == nullptr)
    {
        CloseHandle(g_hMapping);
        g_hMapping = nullptr;
        Napi::Error::New(env, "MapViewOfFile failed").ThrowAsJavaScriptException();
    }

    return env.Undefined();
}

// ---------------------------------------------------------------------------
// close()
// ---------------------------------------------------------------------------
static Napi::Value Close(const Napi::CallbackInfo& info)
{
    if (g_pView != nullptr)
    {
        UnmapViewOfFile(g_pView);
        g_pView = nullptr;
    }
    if (g_hMapping != nullptr)
    {
        CloseHandle(g_hMapping);
        g_hMapping = nullptr;
    }
    return info.Env().Undefined();
}

// ---------------------------------------------------------------------------
// isOpen()
// ---------------------------------------------------------------------------
static Napi::Value IsOpen(const Napi::CallbackInfo& info)
{
    return Napi::Boolean::New(info.Env(), g_pView != nullptr);
}

// ---------------------------------------------------------------------------
// readFrame() → JS object | null
// Implements seqlock-consistent read (max 16 retries before returning null).
// ---------------------------------------------------------------------------
static Napi::Value ReadFrame(const Napi::CallbackInfo& info)
{
    auto env = info.Env();

    // If not open, attempt lazy open (engine may have started since last call)
    if (g_pView == nullptr)
    {
        g_hMapping = OpenFileMappingA(FILE_MAP_READ, FALSE, ARC_SHM_NAME);
        if (g_hMapping != nullptr)
            g_pView = MapViewOfFile(g_hMapping, FILE_MAP_READ, 0, 0, ARC_SHM_SIZE);
        if (g_pView == nullptr)
            return env.Null();
    }

    // Validate magic before seqlock to avoid misreading garbage
    const SHMHeader* hdr = header();
    if (hdr->magic != ARC_SHM_MAGIC)
        return env.Null();

    // Seqlock-consistent copy
    MetricsFrame buf;
    const volatile uint64_t* seqPtr = reinterpret_cast<const volatile uint64_t*>(&hdr->sequence);
    int retries = 16;
    uint64_t seq1, seq2;

    do
    {
        seq1 = *seqPtr;
        if (seq1 & 1)
        {
            // Write in progress — spin briefly
            if (--retries <= 0) return env.Null();
            _mm_pause();
            continue;
        }
        _ReadBarrier(); // compiler memory barrier before copy
        std::memcpy(&buf, frame(), sizeof(MetricsFrame));
        _ReadBarrier();
        seq2 = *seqPtr;
        if (seq1 == seq2) break;
        if (--retries <= 0) return env.Null();
    } while (true);

    // Build JS result object
    auto obj = Napi::Object::New(env);

    obj.Set("sequenceNumber",          Napi::BigInt::New(env, buf.sequenceNumber));
    obj.Set("timestampMs",             Napi::BigInt::New(env, buf.timestampMs));
    obj.Set("peakL",                   Napi::Number::New(env, buf.peakL));
    obj.Set("peakR",                   Napi::Number::New(env, buf.peakR));
    obj.Set("rmsL",                    Napi::Number::New(env, buf.rmsL));
    obj.Set("rmsR",                    Napi::Number::New(env, buf.rmsR));
    obj.Set("peakHoldL",               Napi::Number::New(env, buf.peakHoldL));
    obj.Set("peakHoldR",               Napi::Number::New(env, buf.peakHoldR));
    obj.Set("limiterGainReductionDb",  Napi::Number::New(env, buf.limiterGainReductionDb));
    obj.Set("cpuPercent",              Napi::Number::New(env, buf.cpuPercent));
    obj.Set("cpuPeakPercent",          Napi::Number::New(env, buf.cpuPeakPercent));
    obj.Set("totalLatencySamples",     Napi::Number::New(env, buf.totalLatencySamples));
    obj.Set("totalLatencyMs",          Napi::Number::New(env, buf.totalLatencyMs));
    obj.Set("xrunCount",               Napi::Number::New(env, buf.xrunCount));
    obj.Set("engineRunning",           Napi::Boolean::New(env, buf.engineRunning != 0));
    obj.Set("masterBypassed",          Napi::Boolean::New(env, buf.masterBypassed != 0));
    obj.Set("srcActive",               Napi::Boolean::New(env, buf.srcActive != 0));
    obj.Set("activePluginCount",       Napi::Number::New(env, buf.activePluginCount));
    obj.Set("engineSampleRate",        Napi::Number::New(env, buf.engineSampleRate));
    obj.Set("loopbackSampleRate",      Napi::Number::New(env, buf.loopbackSampleRate));
    obj.Set("bufferSize",              Napi::Number::New(env, buf.bufferSize));

    // Per-slot arrays
    auto slotCpu = Napi::Array::New(env, MAX_PLUGIN_SLOTS);
    auto slotByp = Napi::Array::New(env, MAX_PLUGIN_SLOTS);
    for (int i = 0; i < MAX_PLUGIN_SLOTS; ++i)
    {
        slotCpu.Set(i, Napi::Number::New(env, buf.slotCpuPercent[i]));
        slotByp.Set(i, Napi::Boolean::New(env, buf.slotBypassed[i] != 0));
    }
    obj.Set("slotCpuPercent",  slotCpu);
    obj.Set("slotBypassed",    slotByp);

    // Spectrum — copy into a new Float32Array
    auto specBuf  = Napi::ArrayBuffer::New(env, METRICS_SPECTRUM_BINS * sizeof(float));
    auto specView = Napi::Float32Array::New(env, METRICS_SPECTRUM_BINS, specBuf, 0);
    std::memcpy(specBuf.Data(), buf.spectrum, METRICS_SPECTRUM_BINS * sizeof(float));
    obj.Set("spectrum", specView);

    return obj;
}

// ---------------------------------------------------------------------------
// Module init
// ---------------------------------------------------------------------------
static Napi::Object Init(Napi::Env env, Napi::Object exports)
{
    exports.Set("open",      Napi::Function::New(env, Open));
    exports.Set("close",     Napi::Function::New(env, Close));
    exports.Set("isOpen",    Napi::Function::New(env, IsOpen));
    exports.Set("readFrame", Napi::Function::New(env, ReadFrame));
    return exports;
}

NODE_API_MODULE(shm_reader, Init)
