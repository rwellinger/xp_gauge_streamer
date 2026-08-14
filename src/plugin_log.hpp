/*
 * xp_gauge_streamer - GNS430/530 display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#pragma once

namespace xp_gauge_streamer
{

// Writes one prefixed line to X-Plane's Log.txt.
__attribute__((format(printf, 1, 2))) void log_format(const char *format, ...);

} // namespace xp_gauge_streamer
