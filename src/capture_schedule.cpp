/*
 * xp_gauge_streamer - GNS430/530 display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#include "capture_schedule.hpp"

#include <algorithm>

namespace xp_gauge_streamer
{

namespace
{

// Draw callbacks arrive at multiples of the sim's frame time and so never land
// exactly on the interval. Without this slack a frame that misses by a rounding
// error is deferred to the next one, which drags the rate below the target.
// Far shorter than any frame time, far longer than double rounding.
constexpr double INTERVAL_TOLERANCE_SECONDS = 0.001;

} // namespace

CaptureSchedule::CaptureSchedule(double frames_per_second)
    : minimum_interval_seconds(frames_per_second > 0.0 ? 1.0 / frames_per_second : 0.0)
{
}

std::vector<CaptureSchedule::LastCapture>::iterator CaptureSchedule::find(DeviceId device_id)
{
    return std::find_if(last_captures.begin(), last_captures.end(),
                        [device_id](const LastCapture &last) { return last.device_id == device_id; });
}

bool CaptureSchedule::is_due(DeviceId device_id, double now_seconds)
{
    const auto entry = find(device_id);
    if (entry == last_captures.end())
    {
        last_captures.push_back({device_id, now_seconds});
        return true;
    }

    // A backwards jump means the sim clock was reset (aircraft reload); treat
    // it as due rather than blocking captures until the clock catches up.
    const double elapsed = now_seconds - entry->at_seconds;
    if (elapsed >= 0.0 && elapsed < minimum_interval_seconds - INTERVAL_TOLERANCE_SECONDS)
        return false;

    entry->at_seconds = now_seconds;
    return true;
}

void CaptureSchedule::forget(DeviceId device_id)
{
    const auto entry = find(device_id);
    if (entry != last_captures.end())
        last_captures.erase(entry);
}

} // namespace xp_gauge_streamer
