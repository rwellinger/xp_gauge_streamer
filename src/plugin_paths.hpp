/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#pragma once

#include <string>

namespace xp_gauge_streamer
{

// The plugin folder's "web" directory, served as the HTTP document root.
// Derived from the loaded .xpl at runtime — the folder may sit anywhere,
// including behind XLauncher's "available plugins" indirection.
std::string web_root_path();

// <X-Plane>/Output/xp_gauge_streamer/settings.cfg. Settings live in Output/ so
// a plugin update, which replaces the plugin folder, leaves them alone. The
// directory is created if it does not exist.
std::string settings_file_path();

} // namespace xp_gauge_streamer
