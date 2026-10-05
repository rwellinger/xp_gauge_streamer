/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

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

// The airliner CDU follows the same rule with its own family: the number is the
// unit (captain = FMS, first officer = FMS2), not the model. Both families offer
// the identical 82 suffixes in the installed X-Plane 12.
constexpr char COMMAND_PREFIX_FMS_1[] = "sim/FMS/";
constexpr char COMMAND_PREFIX_FMS_2[] = "sim/FMS2/";

// ToLiss (A319, A320neo, A321, A340) numbers its MCDUs the same way, and the
// screen text datarefs share the prefix: AirbusFBW/MCDU1LSK1L is a key,
// AirbusFBW/MCDU1title a line of the display.
constexpr char COMMAND_PREFIX_TOLISS_MCDU_1[] = "AirbusFBW/MCDU1";
constexpr char COMMAND_PREFIX_TOLISS_MCDU_2[] = "AirbusFBW/MCDU2";

// The Zibo 737 keeps keys and screen apart: laminar/B738/button/fmc1_1L is a
// key, laminar/B738/fmc1/Line01_L a line of the display.
constexpr char COMMAND_PREFIX_ZIBO_FMC_1[] = "laminar/B738/button/fmc1_";
constexpr char COMMAND_PREFIX_ZIBO_FMC_2[] = "laminar/B738/button/fmc2_";
constexpr char DATAREF_PREFIX_ZIBO_FMC_1[] = "laminar/B738/fmc1/";
constexpr char DATAREF_PREFIX_ZIBO_FMC_2[] = "laminar/B738/fmc2/";

// The type names the bezel definition the frontend loads (web/bezels/<type>.json)
// and the button set command_catalog accepts.
constexpr char TYPE_GNS430[]      = "gns430";
constexpr char TYPE_GNS530[]      = "gns530";
constexpr char TYPE_CDU739[]      = "cdu739";
constexpr char TYPE_TOLISS_MCDU[] = "toliss_mcdu";
constexpr char TYPE_ZIBO_FMC[]    = "zibo_fmc";

constexpr ScreenSource FRAMEBUFFER   = ScreenSource::framebuffer;
constexpr ScreenSource TEXT_DATAREFS = ScreenSource::text_datarefs;

constexpr char NO_DATAREFS[] = "";

// Values match XPLMDeviceID: GNS430_1 = 0, GNS430_2 = 1, GNS530_1 = 2,
// GNS530_2 = 3, CDU739_1 = 4, CDU739_2 = 5. Devices X-Plane does not know take
// ids from 100 up, clear of the SDK's enumeration (it ends at MCDU_3 = 24).
std::vector<DeviceDescriptor> &device_table()
{
    static std::vector<DeviceDescriptor> table = {
        {0, "gns430_1", TYPE_GNS430, "GNS 430 Pilot", COMMAND_PREFIX_UNIT_1, NO_DATAREFS, FRAMEBUFFER, true},
        {1, "gns430_2", TYPE_GNS430, "GNS 430 Copilot", COMMAND_PREFIX_UNIT_2, NO_DATAREFS, FRAMEBUFFER, true},
        {2, "gns530_1", TYPE_GNS530, "GNS 530 Pilot", COMMAND_PREFIX_UNIT_1, NO_DATAREFS, FRAMEBUFFER, true},
        {3, "gns530_2", TYPE_GNS530, "GNS 530 Copilot", COMMAND_PREFIX_UNIT_2, NO_DATAREFS, FRAMEBUFFER, true},
        {4, "cdu739_1", TYPE_CDU739, "CDU Captain", COMMAND_PREFIX_FMS_1, NO_DATAREFS, FRAMEBUFFER, true},
        {5, "cdu739_2", TYPE_CDU739, "CDU First Officer", COMMAND_PREFIX_FMS_2, NO_DATAREFS, FRAMEBUFFER, true},
        {100, "toliss_mcdu_1", TYPE_TOLISS_MCDU, "ToLiss MCDU Captain", COMMAND_PREFIX_TOLISS_MCDU_1,
         COMMAND_PREFIX_TOLISS_MCDU_1, TEXT_DATAREFS, true},
        {101, "toliss_mcdu_2", TYPE_TOLISS_MCDU, "ToLiss MCDU First Officer", COMMAND_PREFIX_TOLISS_MCDU_2,
         COMMAND_PREFIX_TOLISS_MCDU_2, TEXT_DATAREFS, true},
        {102, "zibo_fmc_1", TYPE_ZIBO_FMC, "Zibo 737 FMC Captain", COMMAND_PREFIX_ZIBO_FMC_1, DATAREF_PREFIX_ZIBO_FMC_1,
         TEXT_DATAREFS, true},
        {103, "zibo_fmc_2", TYPE_ZIBO_FMC, "Zibo 737 FMC First Officer", COMMAND_PREFIX_ZIBO_FMC_2,
         DATAREF_PREFIX_ZIBO_FMC_2, TEXT_DATAREFS, true},
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
