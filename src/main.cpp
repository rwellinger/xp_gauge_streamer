#include "avionics_capture.hpp"
#include "frame_pipeline.hpp"
#include "plugin_log.hpp"

#include <XPLM/XPLMPlugin.h>
#include <XPLM/XPLMUtilities.h>
#include <civetweb.h>
#include <turbojpeg.h>

#include <cstdio>

using namespace xp_gauge_streamer;

namespace
{

constexpr char PLUGIN_NAME[]      = "xp_gauge_streamer";
constexpr char PLUGIN_SIGNATURE[] = "ch.thwelly.xp_gauge_streamer";
constexpr char PLUGIN_DESCRIPTION[] =
    "Streams the GNS430/530 display to a web frontend and forwards clicks back into the sim.";

// X-Plane's plugin API hands out fixed 256-byte buffers for name/signature/description.
constexpr size_t XPLM_STRING_BUFFER_SIZE = 256;

void log_dependency_versions()
{
    log_format("version %s, civetweb %s, libjpeg-turbo %d.%d.%d", XP_GAUGE_STREAMER_VERSION, mg_version(),
               TURBOJPEG_VERSION_NUMBER / 1000000, (TURBOJPEG_VERSION_NUMBER / 1000) % 1000,
               TURBOJPEG_VERSION_NUMBER % 1000);
}

} // namespace

PLUGIN_API int XPluginStart(char *outName, char *outSignature, char *outDescription)
{
    // Without this the SDK hands back legacy HFS paths ("Macintosh HD:Users:…")
    // that no file API on this platform accepts.
    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);

    std::snprintf(outName, XPLM_STRING_BUFFER_SIZE, "%s", PLUGIN_NAME);
    std::snprintf(outSignature, XPLM_STRING_BUFFER_SIZE, "%s", PLUGIN_SIGNATURE);
    std::snprintf(outDescription, XPLM_STRING_BUFFER_SIZE, "%s", PLUGIN_DESCRIPTION);

    log_dependency_versions();
    return 1;
}

PLUGIN_API void XPluginStop(void) {}

PLUGIN_API int XPluginEnable(void)
{
    start_pipeline();
    start_capture(publish_frame);
    return 1;
}

// Capture first: once no draw callback can fire, nothing can publish into a
// pipeline that is shutting down.
PLUGIN_API void XPluginDisable(void)
{
    stop_capture();
    stop_pipeline();
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID from, int message, void *param) {}
