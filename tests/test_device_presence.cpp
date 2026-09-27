/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#include "device_presence.hpp"

#include <catch_amalgamated.hpp>

using namespace xp_gauge_streamer;

TEST_CASE("an unreported device is absent", "[device_presence]") { CHECK_FALSE(device_is_in_aircraft(42)); }

TEST_CASE("presence follows the latest report", "[device_presence]")
{
    set_device_present(100, true);
    CHECK(device_is_in_aircraft(100));

    set_device_present(100, true);
    set_device_present(100, false);
    CHECK_FALSE(device_is_in_aircraft(100));
}

TEST_CASE("devices are reported independently", "[device_presence]")
{
    set_device_present(4, true);
    set_device_present(101, true);
    set_device_present(4, false);

    CHECK_FALSE(device_is_in_aircraft(4));
    CHECK(device_is_in_aircraft(101));

    set_device_present(101, false);
}
