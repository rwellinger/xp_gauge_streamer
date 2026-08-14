#pragma once

namespace xp_gauge_streamer
{

// Writes one prefixed line to X-Plane's Log.txt.
__attribute__((format(printf, 1, 2))) void log_format(const char *format, ...);

} // namespace xp_gauge_streamer
