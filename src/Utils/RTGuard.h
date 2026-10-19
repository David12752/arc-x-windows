/*
 * ============================================================================
 * ARC System Audio Host - RTGuard
 *
 * Lightweight compile-time-selectable assertions for verifying that code runs
 * on the correct thread class (RT audio callback vs. Non-RT threads).
 *
 * USAGE
 * -----
 *   // Top of any function that must NOT run on the RT callback thread:
 *   ARC_ASSERT_NONRT_SAFE();
 *
 *   // Top of any function that MUST run on the RT callback thread
 *   // (or at least must never touch a mutex/heap):
 *   ARC_ASSERT_RT_SAFE();
 *
 * HOW IT WORKS
 * ------------
 * The macros use a thread_local flag that is set by the ASIO/WASAPI driver
 * glue code when it starts executing the audio callback. Any accidental call
 * to a Non-RT-only function (e.g. one that acquires a mutex or allocates)
 * from within the callback will trip the assertion immediately in Debug builds.
 *
 * In Release builds both macros expand to nothing.
 *
 * MMCSS INTEGRATION
 * -----------------
 * Call `RTGuard::markCurrentThreadAsAudioCallback()` once, at the top of the
 * audio driver callback (or in EngineCore when MMCSS is applied).
 * Call `RTGuard::clearAudioCallbackFlag()` if the same thread later leaves the
 * RT context (rare; mostly useful for unit tests).
 * ============================================================================
 */

#pragma once

#include "JuceHeader.h"

namespace ARC
{

//=============================================================================
class RTGuard
{
public:
    //=========================================================================
    /**
     * Mark the calling thread as an RT audio callback thread.
     * After this call, `ARC_ASSERT_NONRT_SAFE()` will fire jassert if called
     * from this thread.
     *
     * Call once per thread (e.g. from the WASAPI/ASIO callback setup path).
     */
    static void markCurrentThreadAsAudioCallback() noexcept
    {
        isAudioCallbackThread_ = true;
    }

    /** Remove the RT mark from the calling thread. */
    static void clearAudioCallbackFlag() noexcept
    {
        isAudioCallbackThread_ = false;
    }

    /** Returns true if the calling thread is marked as an RT audio thread. */
    static bool isOnAudioCallbackThread() noexcept
    {
        return isAudioCallbackThread_;
    }

private:
    static thread_local bool isAudioCallbackThread_;
};

// Definition lives in RTGuard.cpp
// thread_local bool RTGuard::isAudioCallbackThread_ = false;

} // namespace ARC

//=============================================================================
// ARC_ASSERT_RT_SAFE()
//   Use at the top of functions that are ONLY safe to call on the RT thread.
//   In Debug, asserts that we ARE on the audio callback thread.
//   In Release, expands to nothing.
//
// ARC_ASSERT_NONRT_SAFE()
//   Use at the top of functions that MUST NOT be called from the RT thread
//   (e.g. anything that locks a mutex, allocates heap, or does I/O).
//   In Debug, asserts that we are NOT on the audio callback thread.
//   In Release, expands to nothing.
//=============================================================================

#if defined(JUCE_DEBUG) || defined(_DEBUG) || defined(DEBUG)

    #define ARC_ASSERT_RT_SAFE() \
        jassert(ARC::RTGuard::isOnAudioCallbackThread() && \
                "This function must only be called from the RT audio callback thread.")

    #define ARC_ASSERT_NONRT_SAFE() \
        jassert(!ARC::RTGuard::isOnAudioCallbackThread() && \
                "This function must NOT be called from the RT audio callback thread — "\
                "it may allocate, lock, or perform I/O.")

#else

    #define ARC_ASSERT_RT_SAFE()    /* no-op in Release */
    #define ARC_ASSERT_NONRT_SAFE() /* no-op in Release */

#endif
