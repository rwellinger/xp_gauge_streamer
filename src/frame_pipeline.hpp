/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#pragma once

#include "device_registry.hpp"

#include <cstdint>
#include <vector>

namespace xp_gauge_streamer
{

// Starts one encoder thread per enabled framebuffer device.
void start_pipeline();

// Stops every encoder thread. Call after stop_capture(), so no draw callback
// can publish into a pipeline that is shutting down.
void stop_pipeline();

// Fits FrameSink: takes raw pixels off the draw callback and hands them to the
// device's encoder thread. Copies and returns, never waits.
void publish_frame(DeviceId device_id, const unsigned char *rgb, int width, int height);

// Copies out the most recently encoded frame of a device. False while no frame
// has been encoded yet. `sequence` identifies the frame so a caller can tell
// whether it has seen it before.
bool latest_jpeg(DeviceId device_id, std::vector<unsigned char> &into, std::uint64_t &sequence);

// True once a device has produced at least one frame. A device the aircraft's
// panel does not have never does — that is how the server tells them apart.
bool has_frames(DeviceId device_id);

} // namespace xp_gauge_streamer
