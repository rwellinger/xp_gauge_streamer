/*
 * xp_gauge_streamer - GNS430/530 display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#include "settings.hpp"

#include <catch_amalgamated.hpp>

using namespace xp_gauge_streamer;

TEST_CASE("empty text yields the defaults", "[settings]")
{
    const Settings settings = parse_settings("");

    CHECK(settings.bind_address == "0.0.0.0");
    CHECK(settings.port == 8080);
}

TEST_CASE("known keys are read", "[settings]")
{
    const Settings settings = parse_settings("bind_address = 127.0.0.1\nport = 9090\n");

    CHECK(settings.bind_address == "127.0.0.1");
    CHECK(settings.port == 9090);
}

TEST_CASE("comments, blank lines and unknown keys are ignored", "[settings]")
{
    const Settings settings = parse_settings("# port = 1\n\n  \nquality = 60\nport=9000\n");

    CHECK(settings.port == 9000);
    CHECK(settings.bind_address == "0.0.0.0");
}

TEST_CASE("an unusable port keeps the default", "[settings]")
{
    CHECK(parse_settings("port = eighty").port == 8080);
    CHECK(parse_settings("port = 8080x").port == 8080);
    CHECK(parse_settings("port = 0").port == 8080);
    CHECK(parse_settings("port = 70000").port == 8080);
    CHECK(parse_settings("port =").port == 8080);
}

TEST_CASE("serialized settings parse back unchanged", "[settings]")
{
    Settings written;
    written.bind_address = "192.168.1.10";
    written.port         = 8123;

    const Settings read = parse_settings(serialize_settings(written));

    CHECK(read.bind_address == written.bind_address);
    CHECK(read.port == written.port);
}
