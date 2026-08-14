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

// Registers a draw callback for every device the registry has enabled.
void start_capture(FrameSink sink);

// Re-reads the registry's enabled flags and registers or unregisters devices
// so the callbacks match. Call after toggling a device at runtime.
void refresh_capture_registrations();

void stop_capture();

} // namespace xp_gauge_streamer
