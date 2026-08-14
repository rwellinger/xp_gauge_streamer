/*
 * xp_gauge_streamer - GNS430/530 display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#include "command_catalog.hpp"

#include <catch_amalgamated.hpp>

#include <algorithm>

using namespace xp_gauge_streamer;

TEST_CASE("the unit, not the model, decides the command prefix", "[command_catalog]")
{
    CHECK(command_name("gns430_1", "fpl") == "sim/GPS/g430n1_fpl");
    CHECK(command_name("gns530_1", "fpl") == "sim/GPS/g430n1_fpl");
    CHECK(command_name("gns430_2", "fpl") == "sim/GPS/g430n2_fpl");
    CHECK(command_name("gns530_2", "fpl") == "sim/GPS/g430n2_fpl");
}

TEST_CASE("every whitelisted button maps to a command", "[command_catalog]")
{
    for (const std::string_view button : known_buttons("gns430"))
        CHECK(command_name("gns430_1", button) == std::string("sim/GPS/g430n1_") + std::string(button));

    for (const std::string_view button : known_buttons("gns530"))
        CHECK(command_name("gns530_1", button) == std::string("sim/GPS/g430n1_") + std::string(button));
}

TEST_CASE("the whitelist follows the device type", "[command_catalog]")
{
    // The 530 has a VNAV key, the 430 has not — same command family, different
    // keys, so the whitelist cannot be global.
    CHECK(command_name("gns530_1", "vnav") == "sim/GPS/g430n1_vnav");
    CHECK(command_name("gns430_1", "vnav").empty());

    CHECK(known_buttons("gns430").size() + 1 == known_buttons("gns530").size());
}

TEST_CASE("an unknown device type has no buttons at all", "[command_catalog]")
{
    CHECK(known_buttons("g1000").empty());
    CHECK(known_buttons("").empty());
}

TEST_CASE("unknown buttons are rejected", "[command_catalog]")
{
    CHECK(command_name("gns430_1", "selfdestruct").empty());
    CHECK(command_name("gns430_1", "").empty());
    CHECK(command_name("gns430_1", "FPL").empty());
    CHECK(command_name("gns430_1", "fpl; rm -rf /").empty());
}

TEST_CASE("unknown devices are rejected", "[command_catalog]")
{
    CHECK(command_name("g1000_1", "fpl").empty());
    CHECK(command_name("", "fpl").empty());
}

TEST_CASE("the popout commands stay out of reach", "[command_catalog]")
{
    // They open X-Plane's own GNS window, which is what this plugin replaces.
    CHECK(command_name("gns430_1", "popout").empty());
    CHECK(command_name("gns430_1", "popup").empty());
}
