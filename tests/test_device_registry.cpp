/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#include "device_registry.hpp"

#include <catch_amalgamated.hpp>

using namespace xp_gauge_streamer;

TEST_CASE("slug lookup finds the matching descriptor", "[device_registry]")
{
    const DeviceDescriptor *device = find_device_by_slug("gns530_2");

    REQUIRE(device != nullptr);
    CHECK(device->device_id == 3);
    CHECK(device->display_name == "GNS 530 Copilot");
}

TEST_CASE("unknown slug yields nothing", "[device_registry]")
{
    CHECK(find_device_by_slug("g1000_1") == nullptr);
    CHECK(find_device_by_slug("") == nullptr);
}

TEST_CASE("device id lookup finds the matching descriptor", "[device_registry]")
{
    const DeviceDescriptor *device = find_device_by_id(0);

    REQUIRE(device != nullptr);
    CHECK(device->slug == "gns430_1");
    CHECK(find_device_by_id(99) == nullptr);
}

TEST_CASE("command prefix follows the unit, not the model", "[device_registry]")
{
    const DeviceDescriptor *gns430_pilot   = find_device_by_id(0);
    const DeviceDescriptor *gns430_copilot = find_device_by_id(1);
    const DeviceDescriptor *gns530_pilot   = find_device_by_id(2);
    const DeviceDescriptor *gns530_copilot = find_device_by_id(3);

    REQUIRE(gns430_pilot != nullptr);
    REQUIRE(gns430_copilot != nullptr);
    REQUIRE(gns530_pilot != nullptr);
    REQUIRE(gns530_copilot != nullptr);

    CHECK(gns430_pilot->command_prefix == "sim/GPS/g430n1_");
    CHECK(gns530_pilot->command_prefix == "sim/GPS/g430n1_");
    CHECK(gns430_copilot->command_prefix == "sim/GPS/g430n2_");
    CHECK(gns530_copilot->command_prefix == "sim/GPS/g430n2_");
}

TEST_CASE("the type names the model, the slug the instance", "[device_registry]")
{
    CHECK(find_device_by_slug("gns430_1")->type == "gns430");
    CHECK(find_device_by_slug("gns430_2")->type == "gns430");
    CHECK(find_device_by_slug("gns530_1")->type == "gns530");
    CHECK(find_device_by_slug("gns530_2")->type == "gns530");
    CHECK(find_device_by_slug("cdu739_1")->type == "cdu739");
    CHECK(find_device_by_slug("cdu739_2")->type == "cdu739");
    CHECK(find_device_by_slug("toliss_mcdu_1")->type == "toliss_mcdu");
    CHECK(find_device_by_slug("toliss_mcdu_2")->type == "toliss_mcdu");
    CHECK(find_device_by_slug("zibo_fmc_1")->type == "zibo_fmc");
    CHECK(find_device_by_slug("zibo_fmc_2")->type == "zibo_fmc");
}

TEST_CASE("the ToLiss MCDUs are text devices outside the SDK's id range", "[device_registry]")
{
    const DeviceDescriptor *captain       = find_device_by_slug("toliss_mcdu_1");
    const DeviceDescriptor *first_officer = find_device_by_slug("toliss_mcdu_2");

    REQUIRE(captain != nullptr);
    REQUIRE(first_officer != nullptr);

    CHECK(captain->source == ScreenSource::text_datarefs);
    CHECK(first_officer->source == ScreenSource::text_datarefs);
    CHECK(captain->command_prefix == "AirbusFBW/MCDU1");
    CHECK(first_officer->command_prefix == "AirbusFBW/MCDU2");
    CHECK(captain->dataref_prefix == "AirbusFBW/MCDU1");
    CHECK(first_officer->dataref_prefix == "AirbusFBW/MCDU2");

    // XPLMDeviceID 18 and 19 are X-Plane's own MCDU, not the ToLiss one.
    CHECK(captain->device_id >= 100);
    CHECK(first_officer->device_id >= 100);
}

TEST_CASE("the Zibo FMCs keep keys and screen under different prefixes", "[device_registry]")
{
    const DeviceDescriptor *captain       = find_device_by_slug("zibo_fmc_1");
    const DeviceDescriptor *first_officer = find_device_by_slug("zibo_fmc_2");

    REQUIRE(captain != nullptr);
    REQUIRE(first_officer != nullptr);

    CHECK(captain->source == ScreenSource::text_datarefs);
    CHECK(first_officer->source == ScreenSource::text_datarefs);
    CHECK(captain->command_prefix == "laminar/B738/button/fmc1_");
    CHECK(first_officer->command_prefix == "laminar/B738/button/fmc2_");
    CHECK(captain->dataref_prefix == "laminar/B738/fmc1/");
    CHECK(first_officer->dataref_prefix == "laminar/B738/fmc2/");
    CHECK(captain->device_id >= 100);
    CHECK(first_officer->device_id >= 100);
}

TEST_CASE("Laminar's units are read from the framebuffer", "[device_registry]")
{
    for (const DeviceDescriptor &device : all_devices())
    {
        if (device.type == "toliss_mcdu" || device.type == "zibo_fmc")
            continue;
        CHECK(device.source == ScreenSource::framebuffer);
        CHECK(device.dataref_prefix.empty());
    }
}

TEST_CASE("the CDU units carry their own command family", "[device_registry]")
{
    const DeviceDescriptor *captain       = find_device_by_id(4);
    const DeviceDescriptor *first_officer = find_device_by_id(5);

    REQUIRE(captain != nullptr);
    REQUIRE(first_officer != nullptr);

    CHECK(captain->slug == "cdu739_1");
    CHECK(first_officer->slug == "cdu739_2");

    // Same rule as the GNS units: the number follows the unit, not the model.
    CHECK(captain->command_prefix == "sim/FMS/");
    CHECK(first_officer->command_prefix == "sim/FMS2/");
}

TEST_CASE("every descriptor has a unique slug and device id", "[device_registry]")
{
    for (const DeviceDescriptor &device : all_devices())
    {
        CHECK(find_device_by_slug(device.slug) == &device);
        CHECK(find_device_by_id(device.device_id) == &device);
    }
}

TEST_CASE("toggling the enabled flag takes effect", "[device_registry]")
{
    REQUIRE(set_device_enabled("gns430_2", false));
    CHECK_FALSE(find_device_by_slug("gns430_2")->enabled);
    // Neighbours stay untouched.
    CHECK(find_device_by_slug("gns430_1")->enabled);

    REQUIRE(set_device_enabled("gns430_2", true));
    CHECK(find_device_by_slug("gns430_2")->enabled);

    CHECK_FALSE(set_device_enabled("g1000_1", false));
}
