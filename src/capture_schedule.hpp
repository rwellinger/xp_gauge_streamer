/*
 * xp_gauge_streamer - GNS430/530 display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#pragma once

#include "device_registry.hpp"

#include <vector>

namespace xp_gauge_streamer
{

// X-Plane calls the avionics draw callback at full frame rate, but the GNS
// screen changes far more slowly — this caps the readback rate per device.
class CaptureSchedule
{
  public:
    explicit CaptureSchedule(double frames_per_second);

    // True when the device is due for a readback; records the timestamp then.
    bool is_due(DeviceId device_id, double now_seconds);

    // Drops a device's timestamp so it captures immediately when re-enabled.
    void forget(DeviceId device_id);

  private:
    struct LastCapture
    {
        DeviceId device_id;
        double   at_seconds;
    };

    std::vector<LastCapture>::iterator find(DeviceId device_id);

    double                   minimum_interval_seconds;
    std::vector<LastCapture> last_captures;
};

} // namespace xp_gauge_streamer
