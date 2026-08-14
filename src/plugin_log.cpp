#include "plugin_log.hpp"

#include <XPLM/XPLMUtilities.h>

#include <cstdarg>
#include <cstdio>

namespace xp_gauge_streamer
{

namespace
{

constexpr char LOG_PREFIX[] = "xp_gauge_streamer";

} // namespace

void log_format(const char *format, ...)
{
    char    message[448];
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    char line[512];
    std::snprintf(line, sizeof(line), "[%s] %s\n", LOG_PREFIX, message);
    XPLMDebugString(line);
}

} // namespace xp_gauge_streamer
