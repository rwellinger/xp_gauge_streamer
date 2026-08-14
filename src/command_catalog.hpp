#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace xp_gauge_streamer
{

// The button names the frontend may send. Everything arriving over the network
// is matched against this list — an unknown name never reaches the sim.
const std::vector<std::string_view> &known_buttons();

// Full command name for a device's button, e.g. ("gns530_1", "fpl") →
// "sim/GPS/g430n1_fpl". Empty when the device slug or the button is unknown.
std::string command_name(std::string_view device_slug, std::string_view button);

} // namespace xp_gauge_streamer
