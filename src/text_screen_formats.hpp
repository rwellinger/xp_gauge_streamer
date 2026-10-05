/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#pragma once

#include "text_screen.hpp"

#include <string_view>

namespace xp_gauge_streamer
{

// ToLiss A319, A320neo, A321, A340: AirbusFBW/MCDU<n>title, ...cont3g, ...
const ScreenFormat &toliss_mcdu_format();

// Zibo 737-800: laminar/B738/fmc<n>/Line00_L, ...Line03_G, ...
const ScreenFormat &zibo_fmc_format();

// The format a registry device type reads its screen in; nullptr for a type
// that offers no screen text.
const ScreenFormat *screen_format_for(std::string_view device_type);

} // namespace xp_gauge_streamer
