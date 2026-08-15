/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

/*
 * Throwaway spike for issue #25: does the production capture path — an avionics
 * after-draw callback reading the framebuffer through a pixel buffer object —
 * work under X-Plane's Vulkan-to-OpenGL bridge on Windows and Linux?
 *
 * Deliberately a single file with no dependency on the plugin's own modules:
 * it must stay buildable after the real code moves on, and it is never merged
 * into a release. It writes to Log.txt and dumps one frame as a .ppm.
 */

#include "spike_opengl.hpp"

#include <XPLM/XPLMDisplay.h>
#include <XPLM/XPLMPlugin.h>
#include <XPLM/XPLMProcessing.h>
#include <XPLM/XPLMUtilities.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{

constexpr char PLUGIN_NAME[]        = "Vulkan Readback Spike";
constexpr char PLUGIN_SIGNATURE[]   = "ch.thwelly.xp_gauge_streamer.spike";
constexpr char PLUGIN_DESCRIPTION[] = "Throwaway probe for issue #25 — measures avionics framebuffer readback.";

// The stock devices the streamer captures: four GNS units and the two CDUs.
// Registering all of them means whatever aircraft the tester loads will hit one.
struct StockDevice
{
    XPLMDeviceID id;
    const char  *name;
};

constexpr std::array<StockDevice, 6> STOCK_DEVICES = {{
    {xplm_device_GNS430_1, "GNS430 pilot"},
    {xplm_device_GNS430_2, "GNS430 copilot"},
    {xplm_device_GNS530_1, "GNS530 pilot"},
    {xplm_device_GNS530_2, "GNS530 copilot"},
    {xplm_device_CDU739_1, "CDU captain"},
    {xplm_device_CDU739_2, "CDU first officer"},
}};

constexpr size_t MEASURED_FRAMES = 100;
constexpr size_t WARMUP_FRAMES   = 10;
constexpr int    DUMP_FRAME      = 50;

// The unit draws a black screen until the aircraft's avionics are powered, and
// 100 frames are over in under two seconds — far too fast for anyone to reach
// for a switch. So the probe watches until real pixels show up and only then
// starts measuring, giving the tester about half a minute at 60 fps.
constexpr int WAIT_FRAMES_LIMIT   = 2000;
constexpr int CONTENT_CHECK_EVERY = 15;

// The plugin captures six times a second, not once per rendered frame. Reading
// every frame leaves the transfer barely 16 ms to finish; at the real rate it
// has 166 ms, which may be the whole difference between a stall and a cheap
// copy. So both are measured, back to back, and the log shows the pair.
constexpr double PLUGIN_CAPTURES_PER_SECOND = 6.0;

std::vector<XPLMAvionicsID> registrations;

// The first device whose callback fires becomes the subject; later callbacks
// from other units are ignored so the timing series stays one device's story.
XPLMDeviceID subject_device = xplm_device_GNS430_1;
const char  *subject_name   = nullptr;

struct Measurement
{
    // Every sample is kept: an average alone hides whether the cost is a steady
    // tax or a few stalls, and that distinction decides whether the capture rate
    // has to become a per-platform setting.
    std::vector<double> samples;
    int                 frames_seen      = 0;
    int                 frames_waited    = 0;
    bool                screen_is_live   = false;
    bool                rate_limited     = false;
    float               last_capture_at  = 0.0F;
    bool                summary_reported = false;
};

Measurement measurement;

struct PixelBufferSlot
{
    GLuint name    = 0;
    size_t bytes   = 0;
    int    width   = 0;
    int    height  = 0;
    bool   pending = false;
};

std::array<PixelBufferSlot, 2> slots;
int                            next_slot = 0;

void log_line(const char *format, ...)
{
    char    message[448];
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    char line[512];
    std::snprintf(line, sizeof(line), "[spike25] %s\n", message);
    XPLMDebugString(line);
}

// Every GL call in this file goes through here: the whole point of the spike is
// knowing *which* step the Vulkan bridge rejects, not that something failed.
bool check_gl(const char *step)
{
    const GLenum error = glGetError();
    if (error == GL_NO_ERROR)
        return true;

    log_line("%s: glGetError = 0x%04X", step, static_cast<unsigned>(error));
    return false;
}

std::string x_plane_root()
{
    char system_path[512] = {};
    XPLMGetSystemPath(system_path);
    return std::string(system_path);
}

// Bottom-up GL rows written top-down, so the dump can be eyeballed in any
// viewer without mentally flipping it.
void write_ppm(const unsigned char *pixels, int width, int height)
{
    const std::string path = x_plane_root() + "spike25_frame.ppm";

    std::FILE *file = std::fopen(path.c_str(), "wb");
    if (file == nullptr)
    {
        log_line("could not open %s for writing", path.c_str());
        return;
    }

    std::fprintf(file, "P6\n%d %d\n255\n", width, height);
    const size_t row_bytes = static_cast<size_t>(width) * 3;
    for (int row = height - 1; row >= 0; --row)
        std::fwrite(pixels + static_cast<size_t>(row) * row_bytes, 1, row_bytes, file);

    std::fclose(file);
    log_line("wrote %s (%dx%d)", path.c_str(), width, height);
}

// An all-black frame has two very different explanations — the unit was simply
// switched off, or the bridge handed back an empty buffer without complaining.
// Saying which one it is here saves shipping the .ppm around to find out.
void report_pixel_content(const unsigned char *pixels, int width, int height)
{
    const size_t  count   = static_cast<size_t>(width) * static_cast<size_t>(height) * 3;
    unsigned char highest = 0;
    size_t        lit     = 0;

    for (size_t index = 0; index < count; ++index)
    {
        if (pixels[index] == 0)
            continue;

        ++lit;
        if (pixels[index] > highest)
            highest = pixels[index];
    }

    if (lit == 0)
    {
        log_line("frame content: every one of %zu bytes is zero — screen off, or the readback is empty", count);
        return;
    }

    log_line("frame content: %zu of %zu bytes non-zero, brightest %u — real pixels arrived", lit, count,
             static_cast<unsigned>(highest));
}

void start_readback(PixelBufferSlot &slot, const GLint *viewport, int width, int height)
{
    const size_t needed_bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 3;

    if (slot.name == 0)
    {
        glGenBuffers(1, &slot.name);
        check_gl("glGenBuffers");
    }

    glBindBuffer(GL_PIXEL_PACK_BUFFER, slot.name);
    check_gl("glBindBuffer");

    if (slot.bytes != needed_bytes)
    {
        glBufferData(GL_PIXEL_PACK_BUFFER, static_cast<GLsizeiptr>(needed_bytes), nullptr, GL_STREAM_READ);
        check_gl("glBufferData");
        slot.bytes   = needed_bytes;
        slot.pending = false;
    }

    glReadPixels(viewport[0], viewport[1], width, height, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    // A multisampled avionics framebuffer answers GL_INVALID_OPERATION here —
    // that is question 3 of the spike and decides whether a resolve blit is needed.
    check_gl("glReadPixels");

    slot.width   = width;
    slot.height  = height;
    slot.pending = true;
}

bool any_pixel_lit(const unsigned char *pixels, size_t count)
{
    for (size_t index = 0; index < count; ++index)
    {
        if (pixels[index] != 0)
            return true;
    }
    return false;
}

// While waiting, the frame is only checked for signs of life — and not on every
// one, since a fully black screen costs a full scan to rule out.
void watch_for_content(const unsigned char *pixels, const PixelBufferSlot &slot)
{
    if (measurement.frames_waited % CONTENT_CHECK_EVERY != 0)
        return;

    const size_t count = static_cast<size_t>(slot.width) * static_cast<size_t>(slot.height) * 3;
    if (!any_pixel_lit(pixels, count))
        return;

    measurement.screen_is_live = true;
    log_line("%s: screen came alive after %d frames — measuring now", subject_name, measurement.frames_waited);
}

void consume_ready_slot(PixelBufferSlot &slot, int frame_number)
{
    if (!slot.pending)
        return;

    glBindBuffer(GL_PIXEL_PACK_BUFFER, slot.name);
    check_gl("glBindBuffer (map)");

    const auto *pixels = static_cast<const unsigned char *>(glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY));
    check_gl("glMapBuffer");

    if (pixels == nullptr)
    {
        log_line("frame %d: glMapBuffer returned null", frame_number);
        slot.pending = false;
        return;
    }

    if (!measurement.screen_is_live)
    {
        watch_for_content(pixels, slot);
    }
    else if (frame_number == DUMP_FRAME)
    {
        report_pixel_content(pixels, slot.width, slot.height);
        write_ppm(pixels, slot.width, slot.height);
    }

    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    check_gl("glUnmapBuffer");

    slot.pending = false;
}

double percentile(const std::vector<double> &sorted, double fraction)
{
    const size_t index = static_cast<size_t>(fraction * static_cast<double>(sorted.size() - 1));
    return sorted[index];
}

void report_summary()
{
    std::vector<double> sorted = measurement.samples;
    std::sort(sorted.begin(), sorted.end());

    double total = 0.0;
    for (double sample : sorted)
        total += sample;

    const char *pace = measurement.rate_limited ? "at 6/s (the plugin's rate)" : "every frame";

    log_line("%s %s: %zu captures, median %.1f us, average %.1f us", subject_name, pace, sorted.size(),
             percentile(sorted, 0.5), total / static_cast<double>(sorted.size()));
    log_line("%s %s: fastest %.1f us, p90 %.1f us, p99 %.1f us, slowest %.1f us", subject_name, pace, sorted.front(),
             percentile(sorted, 0.9), percentile(sorted, 0.99), sorted.back());

    // The opening frames pay for buffer allocation and the first map, which is
    // a startup cost rather than the steady-state price of a capture.
    const size_t warmup       = sorted.size() < WARMUP_FRAMES ? sorted.size() : WARMUP_FRAMES;
    double       warmup_total = 0.0;
    for (size_t index = 0; index < warmup; ++index)
        warmup_total += measurement.samples[index];

    log_line("%s %s: first %zu captures averaged %.1f us", subject_name, pace, warmup,
             warmup_total / static_cast<double>(warmup));

    if (measurement.rate_limited)
    {
        measurement.summary_reported = true;
        log_line("done — copy Log.txt and spike25_frame.ppm out of the X-Plane folder");
        return;
    }

    // Second pass at the rate the plugin actually uses. Takes about 17 seconds.
    measurement.rate_limited = true;
    measurement.samples.clear();
    log_line("now measuring again at 6 captures per second — keep the screen lit for ~20 seconds");
}

void measure_frame(double micros)
{
    measurement.samples.push_back(micros);

    if (measurement.samples.size() >= MEASURED_FRAMES)
        report_summary();
}

void capture_once()
{
    const auto started = std::chrono::steady_clock::now();

    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    check_gl("glGetIntegerv(GL_VIEWPORT)");

    const int width  = viewport[2];
    const int height = viewport[3];
    if (width <= 0 || height <= 0)
    {
        log_line("viewport is %dx%d — nothing to read", width, height);
        return;
    }

    if (measurement.frames_seen == 0 && measurement.frames_waited == 0)
    {
        log_line("%s: viewport %dx%d at origin %d,%d", subject_name, width, height, viewport[0], viewport[1]);
        log_line("waiting for the screen to light up — switch on battery and avionics now");
    }

    GLint saved_alignment = 4;
    GLint saved_binding   = 0;
    glGetIntegerv(GL_PACK_ALIGNMENT, &saved_alignment);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &saved_binding);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    start_readback(slots[next_slot], viewport, width, height);

    PixelBufferSlot &ready = slots[1 - next_slot];
    next_slot              = 1 - next_slot;
    const bool dumping     = ready.pending && measurement.frames_seen == DUMP_FRAME;
    consume_ready_slot(ready, measurement.frames_seen);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(saved_binding));
    glPixelStorei(GL_PACK_ALIGNMENT, saved_alignment);

    const auto elapsed = std::chrono::steady_clock::now() - started;

    if (!measurement.screen_is_live)
    {
        ++measurement.frames_waited;

        if (measurement.frames_waited == WAIT_FRAMES_LIMIT)
        {
            log_line("%s: screen stayed black for %d frames — measuring anyway", subject_name, WAIT_FRAMES_LIMIT);
            log_line("if the unit was visibly lit in the cockpit, the readback itself is coming back empty");
            measurement.screen_is_live = true;
        }
        return;
    }

    ++measurement.frames_seen;

    // Writing 600 KB to disk is not part of what a capture costs, so the frame
    // that dumps the .ppm is timed like the others but kept out of the series.
    if (!dumping)
        measure_frame(std::chrono::duration<double, std::micro>(elapsed).count());
}

bool capture_is_due()
{
    const float now      = XPLMGetElapsedTime();
    const float interval = 1.0F / static_cast<float>(PLUGIN_CAPTURES_PER_SECOND);

    if (now - measurement.last_capture_at < interval)
        return false;

    measurement.last_capture_at = now;
    return true;
}

int draw_after(XPLMDeviceID device_id, int /*is_before*/, void *refcon)
{
    if (measurement.summary_reported)
        return 1;

    if (subject_name == nullptr)
    {
        subject_device = device_id;
        subject_name   = static_cast<const char *>(refcon);
        log_line("first callback from %s — measuring this unit", subject_name);

        if (!load_gl_entry_points())
        {
            log_line("GL 1.5 entry points unavailable — readback impossible on this platform");
            measurement.summary_reported = true;
            return 1;
        }
        log_line("GL entry points resolved (%s)", gl_entry_point_source());
    }

    if (device_id != subject_device)
        return 1;

    if (measurement.rate_limited && !capture_is_due())
        return 1;

    capture_once();
    return 1;
}

void register_devices()
{
    for (const StockDevice &device : STOCK_DEVICES)
    {
        XPLMCustomizeAvionics_t params = {};
        params.structSize              = sizeof(XPLMCustomizeAvionics_t);
        params.deviceId                = device.id;
        params.drawCallbackAfter       = draw_after;
        params.refcon                  = const_cast<char *>(device.name);

        XPLMAvionicsID handle = XPLMRegisterAvionicsCallbacksEx(&params);
        if (handle == nullptr)
        {
            log_line("%s: registration FAILED", device.name);
            continue;
        }

        registrations.push_back(handle);
    }

    log_line("registered %zu of %zu stock devices", registrations.size(), STOCK_DEVICES.size());
    log_line("load an aircraft with a GNS or CDU and let it draw for a few seconds");
}

} // namespace

PLUGIN_API int XPluginStart(char *name, char *signature, char *description)
{
    std::strcpy(name, PLUGIN_NAME);
    std::strcpy(signature, PLUGIN_SIGNATURE);
    std::strcpy(description, PLUGIN_DESCRIPTION);

    log_line("spike for issue #25 starting");
    register_devices();
    return 1;
}

PLUGIN_API void XPluginStop()
{
    for (XPLMAvionicsID handle : registrations)
        XPLMUnregisterAvionicsCallbacks(handle);
    registrations.clear();
}

PLUGIN_API int XPluginEnable() { return 1; }

PLUGIN_API void XPluginDisable() {}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID, int, void *) {}
