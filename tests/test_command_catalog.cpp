/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
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

TEST_CASE("the CDU units draw from the two FMS families", "[command_catalog]")
{
    CHECK(command_name("cdu739_1", "key_A") == "sim/FMS/key_A");
    CHECK(command_name("cdu739_2", "key_A") == "sim/FMS2/key_A");
    CHECK(command_name("cdu739_1", "ls_3r") == "sim/FMS/ls_3r");
    CHECK(command_name("cdu739_2", "ls_3r") == "sim/FMS2/ls_3r");

    for (const std::string_view button : known_buttons("cdu739"))
        CHECK(command_name("cdu739_1", button) == std::string("sim/FMS/") + std::string(button));
}

TEST_CASE("the CDU's letter keys are upper case", "[command_catalog]")
{
    // key_A is the command's name. A lower-case key in a bezel definition would
    // fail silently — rejected here, logged by the dispatch, no press in the sim.
    CHECK(command_name("cdu739_1", "key_a").empty());
    CHECK(command_name("cdu739_1", "key_z").empty());
}

TEST_CASE("the CDU whitelist holds the whole family", "[command_catalog]")
{
    // 12 line selects, 26 letters, 10 digits, 8 editing keys, 24 page and mode
    // keys — every sim/FMS/ suffix except the two popup commands.
    CHECK(known_buttons("cdu739").size() == 80);
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

    // The CDU family spells them differently, for the same reason.
    CHECK(command_name("cdu739_1", "CDU_popup").empty());
    CHECK(command_name("cdu739_1", "CDU_popout").empty());
}

TEST_CASE("the ToLiss MCDUs draw from their unit's AirbusFBW family", "[command_catalog]")
{
    CHECK(command_name("toliss_mcdu_1", "LSK1L") == "AirbusFBW/MCDU1LSK1L");
    CHECK(command_name("toliss_mcdu_2", "LSK1L") == "AirbusFBW/MCDU2LSK1L");
    CHECK(command_name("toliss_mcdu_1", "KeyA") == "AirbusFBW/MCDU1KeyA");
    CHECK(command_name("toliss_mcdu_2", "SlewRight") == "AirbusFBW/MCDU2SlewRight");

    // 12 line selects, 26 letters, 10 digits, 10 editing and brightness keys,
    // 14 page keys — every AirbusFBW/MCDU1 command of the A319.
    CHECK(known_buttons("toliss_mcdu").size() == 72);
}

TEST_CASE("the ToLiss keys are case-sensitive", "[command_catalog]")
{
    CHECK(command_name("toliss_mcdu_1", "keya").empty());
    CHECK(command_name("toliss_mcdu_1", "lsk1l").empty());
    CHECK(command_name("toliss_mcdu_1", "key_A").empty());
}
