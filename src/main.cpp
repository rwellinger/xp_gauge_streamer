/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#include "avionics_capture.hpp"
#include "command_dispatch.hpp"
#include "frame_pipeline.hpp"
#include "http_server.hpp"
#include "plugin_log.hpp"
#include "plugin_paths.hpp"
#include "plugin_ui.hpp"
#include "settings.hpp"
#include "text_screen_source.hpp"

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

constexpr char PLUGIN_NAME[]      = "Welly's Gauge Streamer";
constexpr char PLUGIN_SIGNATURE[] = "ch.thwelly.xp_gauge_streamer";
constexpr char PLUGIN_DESCRIPTION[] =
    "Streams the GNS430/530, airliner CDU, ToLiss MCDU and Zibo 737 FMC displays to a web frontend and forwards "
    "clicks back into the sim.";

// X-Plane's plugin API hands out fixed 256-byte buffers for name/signature/description.
constexpr size_t XPLM_STRING_BUFFER_SIZE = 256;

// Capturing costs main-thread frame time whether or not anybody watches, so it
// only runs while a stream is open. The check has to happen here rather than in
// the HTTP handler: starting capture touches the XPLM API, and civetweb's
// threads must never do that.
constexpr float VIEWER_CHECK_INTERVAL_SECONDS = 1.0f;

bool streaming = false;

void log_dependency_versions()
{
    log_format("version %s, civetweb %s, libjpeg-turbo %d.%d.%d", XP_GAUGE_STREAMER_VERSION, mg_version(),
               TURBOJPEG_VERSION_NUMBER / 1000000, (TURBOJPEG_VERSION_NUMBER / 1000) % 1000,
               TURBOJPEG_VERSION_NUMBER % 1000);
}

// Also the only place allowed to ask the sim which units the aircraft has: the
// selection page reads that answer from the HTTP handler, off the main thread.
float follow_viewers(float, float, int, void *)
{
    refresh_device_presence();

    const bool wanted = active_stream_count() > 0;
    if (wanted != streaming)
    {
        streaming = wanted;
        set_readback_enabled(wanted);
        log_format("capture %s", wanted ? "started — a viewer connected" : "stopped — no viewers left");
    }

    set_text_screen_reading_enabled(active_screen_count() > 0);

    return VIEWER_CHECK_INTERVAL_SECONDS;
}

// A device switched on or off in the UI needs its callback registered or
// dropped, and its encoder thread started or joined.
void devices_changed()
{
    refresh_capture_registrations();
    stop_pipeline();
    start_pipeline();
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

    open_settings(settings_file_path());

    return 1;
}

PLUGIN_API void XPluginStop(void) {}

PLUGIN_API int XPluginEnable(void)
{
    start_dispatch();
    start_pipeline();
    start_capture(publish_frame);
    start_text_screen_source();
    start_ui(devices_changed);

    const Settings    &settings = current_settings();
    const ServerConfig config{settings.bind_address, settings.port, web_root_path()};
    start_server(config);

    XPLMRegisterFlightLoopCallback(follow_viewers, VIEWER_CHECK_INTERVAL_SECONDS, nullptr);
    return 1;
}

PLUGIN_API void XPluginDisable(void)
{
    XPLMUnregisterFlightLoopCallback(follow_viewers, nullptr);

    // Server first: no handler may reach into a pipeline — or a command queue —
    // that is going away. Then capture, so no draw callback can publish into a
    // pipeline that is shutting down.
    stop_server();
    stop_dispatch();
    stop_ui();
    stop_capture();
    stop_text_screen_source();
    stop_pipeline();
    streaming = false;
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID from, int message, void *param) {}
