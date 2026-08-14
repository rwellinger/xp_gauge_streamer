#include "capture_schedule.hpp"

#include <catch_amalgamated.hpp>

using namespace xp_gauge_streamer;

namespace
{
constexpr double TWELVE_FPS_INTERVAL = 1.0 / 12.0;
}

TEST_CASE("the first frame of a device is always due", "[capture_schedule]")
{
    CaptureSchedule schedule(12.0);

    CHECK(schedule.is_due(0, 100.0));
}

TEST_CASE("frames inside the interval are skipped", "[capture_schedule]")
{
    CaptureSchedule schedule(12.0);
    REQUIRE(schedule.is_due(0, 100.0));

    CHECK_FALSE(schedule.is_due(0, 100.0 + TWELVE_FPS_INTERVAL / 2.0));
    CHECK(schedule.is_due(0, 100.0 + TWELVE_FPS_INTERVAL));
}

TEST_CASE("a 60 fps caller is thinned to the configured rate", "[capture_schedule]")
{
    CaptureSchedule schedule(12.0);
    int             captured = 0;

    // One simulated second of draw callbacks at 60 fps.
    for (int frame = 0; frame < 60; ++frame)
    {
        if (schedule.is_due(0, static_cast<double>(frame) / 60.0))
            ++captured;
    }

    CHECK(captured == 12);
}

TEST_CASE("devices are limited independently", "[capture_schedule]")
{
    CaptureSchedule schedule(12.0);
    REQUIRE(schedule.is_due(0, 100.0));

    CHECK(schedule.is_due(2, 100.0));
    CHECK_FALSE(schedule.is_due(0, 100.0));
}

TEST_CASE("a backwards clock jump does not block captures", "[capture_schedule]")
{
    CaptureSchedule schedule(12.0);
    REQUIRE(schedule.is_due(0, 500.0));

    // Aircraft reload resets the sim clock.
    CHECK(schedule.is_due(0, 1.0));
}

TEST_CASE("forgetting a device makes it due again", "[capture_schedule]")
{
    CaptureSchedule schedule(12.0);
    REQUIRE(schedule.is_due(0, 100.0));
    REQUIRE_FALSE(schedule.is_due(0, 100.0));

    schedule.forget(0);

    CHECK(schedule.is_due(0, 100.0));
}

TEST_CASE("a non-positive rate never throttles", "[capture_schedule]")
{
    CaptureSchedule schedule(0.0);

    CHECK(schedule.is_due(0, 100.0));
    CHECK(schedule.is_due(0, 100.0));
}
