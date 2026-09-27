/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#include "device_presence.hpp"

#include <algorithm>
#include <mutex>
#include <vector>

namespace xp_gauge_streamer
{

namespace
{

std::mutex            presence_mutex;
std::vector<DeviceId> present_devices;

} // namespace

void set_device_present(DeviceId device_id, bool present)
{
    const std::lock_guard<std::mutex> lock(presence_mutex);

    const auto entry = std::find(present_devices.begin(), present_devices.end(), device_id);
    const bool known = entry != present_devices.end();

    if (present && !known)
        present_devices.push_back(device_id);
    else if (!present && known)
        present_devices.erase(entry);
}

bool device_is_in_aircraft(DeviceId device_id)
{
    const std::lock_guard<std::mutex> lock(presence_mutex);
    return std::find(present_devices.begin(), present_devices.end(), device_id) != present_devices.end();
}

} // namespace xp_gauge_streamer
