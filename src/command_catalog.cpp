#include "command_catalog.hpp"

#include "device_registry.hpp"

#include <algorithm>

namespace xp_gauge_streamer
{

namespace
{

// Every sim/GPS/g430n*_ suffix the installed X-Plane 12 offers, minus "popout"
// and "popup": those open X-Plane's own GNS window, which is exactly what this
// plugin replaces.
const std::vector<std::string_view> BUTTONS = {
    "cdi",  "chapter_dn", "chapter_up",  "clr",     "coarse_down", "coarse_up", "com_ff",  "cursor",
    "cvol", "cvol_dn",    "cvol_up",     "direct",  "ent",         "fine_down", "fine_up", "fpl",
    "menu", "msg",        "nav_com_tog", "nav_ff",  "obs",         "page_dn",   "page_up", "proc",
    "vnav", "vvol",       "vvol_dn",     "vvol_up", "zoom_in",     "zoom_out",
};

bool is_known_button(std::string_view button)
{
    return std::find(BUTTONS.begin(), BUTTONS.end(), button) != BUTTONS.end();
}

} // namespace

const std::vector<std::string_view> &known_buttons() { return BUTTONS; }

std::string command_name(std::string_view device_slug, std::string_view button)
{
    const DeviceDescriptor *device = find_device_by_slug(device_slug);
    if (device == nullptr || !is_known_button(button))
        return {};

    return std::string(device->command_prefix) + std::string(button);
}

} // namespace xp_gauge_streamer
