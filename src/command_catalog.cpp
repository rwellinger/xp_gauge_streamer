#include "command_catalog.hpp"

#include "device_registry.hpp"

#include <algorithm>

namespace xp_gauge_streamer
{

namespace
{

struct ButtonSet
{
    std::string_view              type;
    std::vector<std::string_view> buttons;
};

// Every sim/GPS/g430n*_ suffix the installed X-Plane 12 offers, minus "popout"
// and "popup": those open X-Plane's own GNS window, which is exactly what this
// plugin replaces. The command family is shared by both models, the keys are
// not — so the whitelist is per device type, not global. A type with a
// different family (G1000, MCP) brings its own set here.
const std::vector<ButtonSet> &button_sets()
{
    static const std::vector<ButtonSet> sets = []
    {
        std::vector<std::string_view> gns430 = {
            "cdi",  "chapter_dn", "chapter_up",  "clr",     "coarse_down", "coarse_up", "com_ff",  "cursor",
            "cvol", "cvol_dn",    "cvol_up",     "direct",  "ent",         "fine_down", "fine_up", "fpl",
            "menu", "msg",        "nav_com_tog", "nav_ff",  "obs",         "page_dn",   "page_up", "proc",
            "vvol", "vvol_dn",    "vvol_up",     "zoom_in", "zoom_out",
        };

        // The only key the 530 has and the 430 has not.
        std::vector<std::string_view> gns530 = gns430;
        gns530.push_back("vnav");

        return std::vector<ButtonSet>{{"gns430", std::move(gns430)}, {"gns530", std::move(gns530)}};
    }();

    return sets;
}

} // namespace

const std::vector<std::string_view> &known_buttons(std::string_view device_type)
{
    static const std::vector<std::string_view> none;

    const std::vector<ButtonSet> &sets = button_sets();
    const auto                    match =
        std::find_if(sets.begin(), sets.end(), [device_type](const ButtonSet &set) { return set.type == device_type; });

    return match != sets.end() ? match->buttons : none;
}

std::string command_name(std::string_view device_slug, std::string_view button)
{
    const DeviceDescriptor *device = find_device_by_slug(device_slug);
    if (device == nullptr)
        return {};

    const std::vector<std::string_view> &buttons = known_buttons(device->type);
    if (std::find(buttons.begin(), buttons.end(), button) == buttons.end())
        return {};

    return std::string(device->command_prefix) + std::string(button);
}

} // namespace xp_gauge_streamer
