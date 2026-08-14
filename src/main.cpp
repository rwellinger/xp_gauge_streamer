#include "avionics_capture.hpp"
#include "command_dispatch.hpp"
#include "frame_pipeline.hpp"
#include "http_server.hpp"
#include "plugin_log.hpp"
#include "plugin_paths.hpp"
#include "settings.hpp"

#include <XPLM/XPLMPlugin.h>
#include <XPLM/XPLMProcessing.h>
#include <XPLM/XPLMUtilities.h>
#include <civetweb.h>
#include <turbojpeg.h>

#include <cstdio>
#include <string>

using namespace xp_gauge_streamer;

namespace
{

constexpr char PLUGIN_NAME[]      = "xp_gauge_streamer";
constexpr char PLUGIN_SIGNATURE[] = "ch.thwelly.xp_gauge_streamer";
constexpr char PLUGIN_DESCRIPTION[] =
    "Streams the GNS430/530 display to a web frontend and forwards clicks back into the sim.";

// X-Plane's plugin API hands out fixed 256-byte buffers for name/signature/description.
constexpr size_t XPLM_STRING_BUFFER_SIZE = 256;

// Capturing costs main-thread frame time whether or not anybody watches, so it
// only runs while a stream is open. The check has to happen here rather than in
// the HTTP handler: starting capture touches the XPLM API, and civetweb's
// threads must never do that.
constexpr float VIEWER_CHECK_INTERVAL_SECONDS = 1.0f;

Settings    settings;
std::string settings_file;
bool        capturing = false;

void log_dependency_versions()
{
    log_format("version %s, civetweb %s, libjpeg-turbo %d.%d.%d", XP_GAUGE_STREAMER_VERSION, mg_version(),
               TURBOJPEG_VERSION_NUMBER / 1000000, (TURBOJPEG_VERSION_NUMBER / 1000) % 1000,
               TURBOJPEG_VERSION_NUMBER % 1000);
}

void set_capturing(bool wanted)
{
    if (wanted == capturing)
        return;

    if (wanted)
    {
        start_pipeline();
        start_capture(publish_frame);
    }
    else
    {
        // Capture first: once no draw callback can fire, nothing can publish
        // into a pipeline that is shutting down.
        stop_capture();
        stop_pipeline();
    }

    capturing = wanted;
    log_format("capture %s", wanted ? "started — a viewer connected" : "stopped — no viewers left");
}

float follow_viewers(float, float, int, void *)
{
    set_capturing(active_stream_count() > 0);
    return VIEWER_CHECK_INTERVAL_SECONDS;
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

    settings_file = settings_file_path();
    settings      = load_settings(settings_file);
    // Writing the defaults back makes the file discoverable — there is no UI
    // for these settings until phase 7.
    save_settings(settings_file, settings);

    return 1;
}

PLUGIN_API void XPluginStop(void) {}

PLUGIN_API int XPluginEnable(void)
{
    start_dispatch();

    const ServerConfig config{settings.bind_address, settings.port, web_root_path()};
    if (!start_server(config))
    {
        stop_dispatch();
        return 0;
    }

    XPLMRegisterFlightLoopCallback(follow_viewers, VIEWER_CHECK_INTERVAL_SECONDS, nullptr);
    return 1;
}

PLUGIN_API void XPluginDisable(void)
{
    XPLMUnregisterFlightLoopCallback(follow_viewers, nullptr);

    // Server first: no handler may reach into a pipeline — or a command queue —
    // that is going away.
    stop_server();
    stop_dispatch();
    set_capturing(false);
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID from, int message, void *param) {}
