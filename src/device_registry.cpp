#include "device_registry.hpp"

#include <algorithm>

namespace xp_gauge_streamer
{

namespace
{

// GNS430 and GNS530 share one command family each — the namespace follows the
// unit (pilot = g430n1_, copilot = g430n2_), not the model. Verified against
// the Commands.txt of the installed X-Plane 12; no sim/GPS/g530_* exists.
constexpr char COMMAND_PREFIX_UNIT_1[] = "sim/GPS/g430n1_";
constexpr char COMMAND_PREFIX_UNIT_2[] = "sim/GPS/g430n2_";

// Values match XPLMDeviceID: GNS430_1 = 0, GNS430_2 = 1, GNS530_1 = 2, GNS530_2 = 3.
std::vector<DeviceDescriptor> &device_table()
{
    static std::vector<DeviceDescriptor> table = {
        {0, "gns430_1", "GNS 430 Pilot", COMMAND_PREFIX_UNIT_1, true},
        {1, "gns430_2", "GNS 430 Copilot", COMMAND_PREFIX_UNIT_2, true},
        {2, "gns530_1", "GNS 530 Pilot", COMMAND_PREFIX_UNIT_1, true},
        {3, "gns530_2", "GNS 530 Copilot", COMMAND_PREFIX_UNIT_2, true},
    };
    return table;
}

std::vector<DeviceDescriptor>::iterator find_entry_by_slug(std::string_view slug)
{
    std::vector<DeviceDescriptor> &table = device_table();
    return std::find_if(table.begin(), table.end(),
                        [slug](const DeviceDescriptor &device) { return device.slug == slug; });
}

} // namespace

const std::vector<DeviceDescriptor> &all_devices() { return device_table(); }

const DeviceDescriptor *find_device_by_slug(std::string_view slug)
{
    const auto match = find_entry_by_slug(slug);
    return match != device_table().end() ? &*match : nullptr;
}

const DeviceDescriptor *find_device_by_id(DeviceId device_id)
{
    const std::vector<DeviceDescriptor> &table = device_table();
    const auto match = std::find_if(table.begin(), table.end(), [device_id](const DeviceDescriptor &device)
                                    { return device.device_id == device_id; });
    return match != table.end() ? &*match : nullptr;
}

bool set_device_enabled(std::string_view slug, bool enabled)
{
    const auto match = find_entry_by_slug(slug);
    if (match == device_table().end())
        return false;

    match->enabled = enabled;
    return true;
}

} // namespace xp_gauge_streamer
