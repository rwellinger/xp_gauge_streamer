/*
 * xp_gauge_streamer - GNS430/530 display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#pragma once

#include <string_view>

namespace xp_gauge_streamer
{

// Resolves every command reference once and registers the flight loop that
// runs queued presses. Call from the main thread.
void start_dispatch();

void stop_dispatch();

// Queues a button press for the main thread. Safe to call from civetweb's
// threads — and the only safe way in, because the XPLM API is not thread-safe.
// False when the device or the button is unknown, or when dispatch is stopped;
// the press is dropped then.
bool press_button(std::string_view device_slug, std::string_view button);

} // namespace xp_gauge_streamer
