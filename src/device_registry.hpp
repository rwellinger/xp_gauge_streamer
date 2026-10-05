/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace xp_gauge_streamer
{

// Mirrors the SDK's XPLMDeviceID (a typedef for int) so this module — and the
// unit tests built from it — never need the X-Plane SDK headers.
using DeviceId = int;

// Where a device's picture comes from. Laminar's own units are read back from
// the framebuffer X-Plane renders them into; an add-on that draws its display
// itself offers no such framebuffer, only its screen text in datarefs.
enum class ScreenSource : std::uint8_t
{
    framebuffer,
    text_datarefs,
};

struct DeviceDescriptor
{
    DeviceId         device_id;
    std::string_view slug;           // URL path segment and instance, e.g. "gns430_1"
    std::string_view type;           // model, e.g. "gns430" — picks bezel and button set
    std::string_view display_name;   // shown in the web frontend
    std::string_view command_prefix; // e.g. "sim/GPS/g430n1_"
    std::string_view dataref_prefix; // screen text datarefs; empty for a framebuffer device
    ScreenSource     source;
    bool             enabled;
};

const std::vector<DeviceDescriptor> &all_devices();

// Both lookups return nullptr when nothing matches.
const DeviceDescriptor *find_device_by_slug(std::string_view slug);
const DeviceDescriptor *find_device_by_id(DeviceId device_id);

// Returns false when the slug is unknown; the table stays untouched then.
bool set_device_enabled(std::string_view slug, bool enabled);

} // namespace xp_gauge_streamer
