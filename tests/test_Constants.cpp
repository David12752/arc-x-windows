/*
 * ============================================================================
 * ARC System Audio Host - Constants Unit Tests
 * ============================================================================
 */

#include <catch2/catch_test_macros.hpp>
#include "Utils/Constants.h"

using namespace ARC::Constants;

TEST_CASE("Constants - Audio Processing", "[Constants]")
{
    REQUIRE(MAX_SAMPLE_RATE > 0.0);
    REQUIRE(MAX_SAMPLE_RATE == 192000.0);
    REQUIRE(MAX_BUFFER_SIZE > 0);
    REQUIRE(MAX_BUFFER_SIZE == 8192);
    REQUIRE(MIN_SAMPLE_RATE > 0.0);
    REQUIRE(MIN_BUFFER_SIZE > 0);
}

TEST_CASE("Constants - CPU Monitoring", "[Constants]")
{
    REQUIRE(CPU_HISTORY_SIZE > 0);
    REQUIRE(CPU_HISTORY_SIZE == 100);
    REQUIRE(DEFAULT_CALLBACK_DURATION_MS > 0.0);
    REQUIRE(HIGH_CPU_THRESHOLD_PERCENT > 0.0);
    REQUIRE(HIGH_CPU_THRESHOLD_PERCENT <= 100.0);
}

TEST_CASE("Constants - Plugin Chain", "[Constants]")
{
    REQUIRE(MAX_PLUGINS > 0);
    REQUIRE(MAX_PLUGINS == 10);
    REQUIRE(PLUGIN_BLACKLIST_FAILURE_COUNT > 0);
}

TEST_CASE("Constants - Error Handling", "[Constants]")
{
    REQUIRE(MAX_CONSECUTIVE_ERRORS > 0);
    REQUIRE(ERROR_LOG_THROTTLE_MS > 0.0);
}

