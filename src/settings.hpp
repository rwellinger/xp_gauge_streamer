#pragma once

#include <string>

namespace xp_gauge_streamer
{

struct Settings
{
    // 0.0.0.0 on purpose: a tablet in the same network is the point of this
    // plugin. See the security note in the README — the server has no auth.
    std::string bind_address = "0.0.0.0";
    int         port         = 8080;
};

// Reads "key = value" lines and ignores blanks, comments ('#') and unknown
// keys. A malformed value keeps the default for that key.
Settings parse_settings(const std::string &text);

std::string serialize_settings(const Settings &settings);

// A missing or unreadable file yields the defaults.
Settings load_settings(const std::string &path);

bool save_settings(const std::string &path, const Settings &settings);

} // namespace xp_gauge_streamer
