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
