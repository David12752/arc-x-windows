/*
 * ============================================================================
 * ARC System Audio Host - Constants Header
 * Centralized constants to replace magic numbers throughout the codebase
 * ============================================================================
 */

#pragma once

namespace ARC
{
namespace Constants
{

//=============================================================================
// Audio Processing Constants
//=============================================================================

/** Maximum sample rate supported (192 kHz) */
constexpr double MAX_SAMPLE_RATE = 192000.0;

/** Maximum buffer size supported (8192 samples) */
constexpr int MAX_BUFFER_SIZE = 8192;

/** Minimum sample rate supported (8 kHz) */
constexpr double MIN_SAMPLE_RATE = 8000.0;

/** Minimum buffer size supported (32 samples) */
constexpr int MIN_BUFFER_SIZE = 32;

//=============================================================================
// CPU Monitoring Constants
//=============================================================================

/** Number of CPU usage samples to keep in history */
constexpr int CPU_HISTORY_SIZE = 100;

/** Default callback duration for CPU calculation (5ms) */
constexpr double DEFAULT_CALLBACK_DURATION_MS = 5.0;

/** High CPU threshold percentage (80%) */
constexpr double HIGH_CPU_THRESHOLD_PERCENT = 80.0;

/** Maximum number of consecutive errors before auto-bypass */
constexpr int MAX_CONSECUTIVE_ERRORS = 10;

//=============================================================================
// Plugin Chain Constants
//=============================================================================

/** Maximum number of plugins in chain */
constexpr int MAX_PLUGINS = 10;

/** Number of plugin failures before blacklisting */
constexpr int PLUGIN_BLACKLIST_FAILURE_COUNT = 3;

//=============================================================================
// Audio Level Metering Constants
//=============================================================================

/** Peak decay coefficient (60dB/s decay at 48kHz) */
constexpr float PEAK_DECAY_COEFFICIENT = 0.9995f;

/** RMS decay coefficient */
constexpr float RMS_DECAY_COEFFICIENT = 0.999f;

/** Peak hold time in samples (~1 second at 48kHz) */
constexpr int PEAK_HOLD_TIME_SAMPLES = 48000;

//=============================================================================
// Spectrum Analyzer Constants
//=============================================================================

/** FFT size for spectrum analysis (2048 samples) */
constexpr int FFT_SIZE = 2048;

/** Number of frequency bins (FFT_SIZE / 2) */
constexpr int NUM_FFT_BINS = FFT_SIZE / 2;

/** Spectrum smoothing factor */
constexpr float SPECTRUM_SMOOTHING_FACTOR = 0.8f;

/** Minimum magnitude to avoid log(0) */
constexpr float MIN_MAGNITUDE = 0.00001f;

//=============================================================================
// Limiter Constants
//=============================================================================

/** Look-ahead samples for limiter (64 samples) */
constexpr int LIMITER_LOOKAHEAD_SAMPLES = 64;

/** Limiter attack time (1ms) */
constexpr float LIMITER_ATTACK_MS = 0.001f;

/** Limiter release time (100ms) */
constexpr float LIMITER_RELEASE_MS = 100.0f;

//=============================================================================
// Gain Smoothing Constants
//=============================================================================

/** Gain smoothing time (20ms) */
constexpr double GAIN_SMOOTHING_TIME_MS = 20.0;

//=============================================================================
// Error Logging Constants
//=============================================================================

/** Minimum time between error logs (1 second) */
constexpr double ERROR_LOG_THROTTLE_MS = 1000.0;

} // namespace Constants
} // namespace ARC

