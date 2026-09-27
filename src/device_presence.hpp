/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#pragma once

#include "device_registry.hpp"

namespace xp_gauge_streamer
{

// Which devices the current aircraft has. Each screen source answers for its own
// devices from the main thread; the HTTP layer and the settings window read the
// answer from wherever they run. SDK-free, so safe from any thread.
void set_device_present(DeviceId device_id, bool present);

// False for a device nobody has reported yet.
bool device_is_in_aircraft(DeviceId device_id);

} // namespace xp_gauge_streamer
