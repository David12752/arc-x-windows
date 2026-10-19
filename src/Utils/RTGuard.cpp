/*
 * ============================================================================
 * ARC System Audio Host - RTGuard Implementation
 * ============================================================================
 */

#include "RTGuard.h"

namespace ARC
{

thread_local bool RTGuard::isAudioCallbackThread_ = false;

} // namespace ARC
