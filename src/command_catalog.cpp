/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

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
//
// The CDU's sim/FMS*/ family follows the same rule, with two things to watch:
// its CDU_popup and CDU_popout are excluded for the reason above, and the
// letter keys are spelled key_A to key_Z — upper case is the command's name,
// and a lower-case one is silently rejected here.
//
// The list is the full family, not what one bezel draws: keys without a 737
// counterpart stay in so a later Airbus or AW139 layout needs a JSON file and
// no code.
//
// The ToLiss MCDU's AirbusFBW/MCDU<n> family is taken whole from the installed
// A319 except UndockMCDU<n>, which sits outside the prefix anyway. Its letter
// keys are KeyA to KeyZ, again case-sensitive.
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

        std::vector<std::string_view> cdu739 = {
            "ls_1l",      "ls_2l",     "ls_3l",     "ls_4l",     "ls_5l",    "ls_6l",      "ls_1r",     "ls_2r",
            "ls_3r",      "ls_4r",     "ls_5r",     "ls_6r",     "key_A",    "key_B",      "key_C",     "key_D",
            "key_E",      "key_F",     "key_G",     "key_H",     "key_I",    "key_J",      "key_K",     "key_L",
            "key_M",      "key_N",     "key_O",     "key_P",     "key_Q",    "key_R",      "key_S",     "key_T",
            "key_U",      "key_V",     "key_W",     "key_X",     "key_Y",    "key_Z",      "key_0",     "key_1",
            "key_2",      "key_3",     "key_4",     "key_5",     "key_6",    "key_7",      "key_8",     "key_9",
            "key_period", "key_minus", "key_slash", "key_space", "key_back", "key_delete", "key_clear", "key_overfly",
            "index",      "fpln",      "clb",       "crz",       "des",      "dir_intc",   "legs",      "dep_arr",
            "hold",       "prog",      "exec",      "fix",       "navrad",   "airport",    "up",        "down",
            "perf",       "fuel_pred", "data",      "menu",      "sec_fpln", "atc_comm",   "prev",      "next",
        };

        std::vector<std::string_view> toliss_mcdu = {
            "LSK1L",     "LSK2L",   "LSK3L", "LSK4L",      "LSK5L",   "LSK6L",    "LSK1R",    "LSK2R",    "LSK3R",
            "LSK4R",     "LSK5R",   "LSK6R", "KeyA",       "KeyB",    "KeyC",     "KeyD",     "KeyE",     "KeyF",
            "KeyG",      "KeyH",    "KeyI",  "KeyJ",       "KeyK",    "KeyL",     "KeyM",     "KeyN",     "KeyO",
            "KeyP",      "KeyQ",    "KeyR",  "KeyS",       "KeyT",    "KeyU",     "KeyV",     "KeyW",     "KeyX",
            "KeyY",      "KeyZ",    "Key0",  "Key1",       "Key2",    "Key3",     "Key4",     "Key5",     "Key6",
            "Key7",      "Key8",    "Key9",  "KeyDecimal", "KeyPM",   "KeySlash", "KeySpace", "KeyClear", "KeyOverfly",
            "KeyBright", "KeyDim",  "DirTo", "Prog",       "Perf",    "Init",     "Data",     "Fpln",     "RadNav",
            "FuelPred",  "SecFpln", "ATC",   "Menu",       "Airport", "SlewUp",   "SlewDown", "SlewLeft", "SlewRight",
        };

        return std::vector<ButtonSet>{{"gns430", std::move(gns430)},
                                      {"gns530", std::move(gns530)},
                                      {"cdu739", std::move(cdu739)},
                                      {"toliss_mcdu", std::move(toliss_mcdu)}};
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
