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
#include <string>

namespace xp_gauge_streamer
{

// Registers the flight loop that watches the registry's text devices — the
// ToLiss MCDUs and the Zibo FMCs: whether the loaded aircraft offers their
// datarefs, and — while reading is enabled — what their screens show. Call
// from the main thread.
void start_text_screen_source();

void stop_text_screen_source();

// The screens are only read while somebody watches. Presence is tracked
// regardless. Main thread only.
void set_text_screen_reading_enabled(bool enabled);

// Copies out a unit's newest screen as JSON (see TextScreen::to_json). False
// until one has been read. `sequence` changes only when the screen does. Safe
// from any thread.
bool latest_text_screen(DeviceId device_id, std::string &json, std::uint64_t &sequence);

} // namespace xp_gauge_streamer
