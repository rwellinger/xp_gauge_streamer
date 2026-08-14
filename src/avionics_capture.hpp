/*
 * xp_gauge_streamer - GNS430/530 display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#pragma once

#include "device_registry.hpp"

namespace xp_gauge_streamer
{

// Receives one raw RGB frame straight from the draw callback. Rows run
// bottom-up, the way OpenGL hands them over. The buffer belongs to the caller
// and is reused after the call returns — the sink must copy what it keeps.
//
// Runs on X-Plane's main thread while rendering is blocked: copy and return.
// No encoding, no I/O.
using FrameSink = void (*)(DeviceId device_id, const unsigned char *rgb, int width, int height);

// Registers a draw callback for every device the registry has enabled. The
// callbacks stay registered while the plugin runs — they are what tells the
// plugin which devices the aircraft has. Whether they read pixels back is
// set_readback_enabled()'s business.
void start_capture(FrameSink sink);

// Reading pixels back is the expensive part, so it only runs while somebody
// watches. Registration is unaffected.
void set_readback_enabled(bool enabled);

// Asks the sim which registered devices the current aircraft actually has and
// stores the answer. Main thread only — call it regularly, an aircraft change
// swaps the whole panel.
void refresh_device_presence();

// True when the current aircraft uses the device, as of the last
// refresh_device_presence(). False for a device that is switched off in the
// registry, since nothing is registered for it then. Safe from any thread.
bool device_is_in_aircraft(DeviceId device_id);

// Re-reads the registry's enabled flags and registers or unregisters devices
// so the callbacks match. Call after toggling a device at runtime.
void refresh_capture_registrations();

void stop_capture();

} // namespace xp_gauge_streamer
