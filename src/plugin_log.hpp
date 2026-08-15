/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#pragma once

// Lets the compiler type-check the varargs against the format string. GCC and
// Clang understand it; MSVC has no equivalent and would reject the attribute.
#if defined(__GNUC__) || defined(__clang__)
#define XP_GAUGE_STREAMER_PRINTF_FORMAT(format_index, first_argument_index)                                            \
    __attribute__((format(printf, format_index, first_argument_index)))
#else
#define XP_GAUGE_STREAMER_PRINTF_FORMAT(format_index, first_argument_index)
#endif

namespace xp_gauge_streamer
{

// Writes one prefixed line to X-Plane's Log.txt.
XP_GAUGE_STREAMER_PRINTF_FORMAT(1, 2) void log_format(const char *format, ...);

} // namespace xp_gauge_streamer
