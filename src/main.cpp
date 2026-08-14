#include <XPLM/XPLMPlugin.h>
#include <XPLM/XPLMUtilities.h>
#include <civetweb.h>
#include <cstdio>
#include <turbojpeg.h>

namespace
{

constexpr char PLUGIN_NAME[]      = "xp_gauge_streamer";
constexpr char PLUGIN_SIGNATURE[] = "ch.thwelly.xp_gauge_streamer";
constexpr char PLUGIN_DESCRIPTION[] =
    "Streams the GNS430/530 display to a web frontend and forwards clicks back into the sim.";

// X-Plane's plugin API hands out fixed 256-byte buffers for name/signature/description.
constexpr size_t XPLM_STRING_BUFFER_SIZE = 256;

void log_line(const char *message)
{
    char line[512];
    std::snprintf(line, sizeof(line), "[%s] %s\n", PLUGIN_NAME, message);
    XPLMDebugString(line);
}

void log_dependency_versions()
{
    char line[256];
    std::snprintf(line, sizeof(line), "version %s, civetweb %s, libjpeg-turbo %d.%d.%d", XP_GAUGE_STREAMER_VERSION,
                  mg_version(), TURBOJPEG_VERSION_NUMBER / 1000000, (TURBOJPEG_VERSION_NUMBER / 1000) % 1000,
                  TURBOJPEG_VERSION_NUMBER % 1000);
    log_line(line);
}

} // namespace

PLUGIN_API int XPluginStart(char *outName, char *outSignature, char *outDescription)
{
    std::snprintf(outName, XPLM_STRING_BUFFER_SIZE, "%s", PLUGIN_NAME);
    std::snprintf(outSignature, XPLM_STRING_BUFFER_SIZE, "%s", PLUGIN_SIGNATURE);
    std::snprintf(outDescription, XPLM_STRING_BUFFER_SIZE, "%s", PLUGIN_DESCRIPTION);

    log_dependency_versions();
    return 1;
}

PLUGIN_API void XPluginStop(void) {}

PLUGIN_API int XPluginEnable(void) { return 1; }

PLUGIN_API void XPluginDisable(void) {}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID from, int message, void *param) {}
