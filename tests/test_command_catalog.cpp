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
    for (const std::string_view button : known_buttons())
        CHECK(command_name("gns430_1", button) == std::string("sim/GPS/g430n1_") + std::string(button));
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
