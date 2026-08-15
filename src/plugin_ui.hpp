/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#pragma once

namespace xp_gauge_streamer
{

// Called after the user switched a device on or off, so capture registrations
// and encoder threads can follow. The UI reads and draws; keeping the plugin's
// moving parts in sync is not its job.
using DevicesChanged = void (*)();

// Adds the plugins menu entry and prepares ImGui. Call from XPluginEnable.
void start_ui(DevicesChanged on_devices_changed);

void stop_ui();

} // namespace xp_gauge_streamer
