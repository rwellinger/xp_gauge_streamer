/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#include "plugin_about.hpp"

#include <wellys_about.hpp>

namespace xp_gauge_streamer
{

namespace
{

// What ships inside the .xpl. Catch2 only builds the tests, so it is not here.
const WellysAbout::PluginInfo &plugin_info()
{
    static const WellysAbout::PluginInfo info{
        "Welly's Gauge Streamer",
        XP_GAUGE_STREAMER_VERSION,
        "Copyright (c) 2026 thWelly",
        "Released under the MIT License, without any warranty.",
        {
            {"Dear ImGui", "MIT"},
            {"civetweb", "MIT"},
            {"libjpeg-turbo", "IJG, BSD-3-Clause, zlib"},
            {"nlohmann/json", "MIT"},
            {"X-Plane SDK", "Laminar Research SDK License"},
        },
    };
    return info;
}

} // namespace

void draw_about_window(bool &open) { WellysAbout::draw_window(open, plugin_info(), WellysAbout::photo_texture()); }

void release_about() { WellysAbout::release_photo(); }

} // namespace xp_gauge_streamer
