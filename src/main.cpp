#include "avionics_capture.hpp"
#include "plugin_log.hpp"

#include <XPLM/XPLMPlugin.h>
#include <XPLM/XPLMProcessing.h>
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

constexpr double FRAME_REPORT_INTERVAL_SECONDS = 5.0;

// Stand-in until the frame_buffer of phase 3 (issue #5) takes the frames. It
// only counts them, which is what verifies the capture rate.
struct FrameCounter
{
    long   frames         = 0;
    double last_report_at = 0.0;
} frame_counter;

void count_frame(DeviceId device_id, const unsigned char *rgb, int width, int height)
{
    ++frame_counter.frames;

    const double now     = XPLMGetElapsedTime();
    const double elapsed = now - frame_counter.last_report_at;
    if (elapsed < FRAME_REPORT_INTERVAL_SECONDS)
        return;

    log_format("capture rate: %.1f frames/s across all devices", static_cast<double>(frame_counter.frames) / elapsed);
    frame_counter.frames         = 0;
    frame_counter.last_report_at = now;
}

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
    frame_counter.frames         = 0;
    frame_counter.last_report_at = XPLMGetElapsedTime();
    start_capture(count_frame);
    return 1;
}

PLUGIN_API void XPluginDisable(void) { stop_capture(); }

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID from, int message, void *param) {}
