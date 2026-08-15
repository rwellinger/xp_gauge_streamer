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
#include <XPLM/XPLMUtilities.h>

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

constexpr int MEASURED_FRAMES = 100;
constexpr int DUMP_FRAME      = 50;

std::vector<XPLMAvionicsID> registrations;

// The first device whose callback fires becomes the subject; later callbacks
// from other units are ignored so the timing series stays one device's story.
XPLMDeviceID subject_device = xplm_device_GNS430_1;
const char  *subject_name   = nullptr;

struct Measurement
{
    int    frame_count      = 0;
    double total_micros     = 0.0;
    double slowest_micros   = 0.0;
    double fastest_micros   = 0.0;
    bool   summary_reported = false;
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

    if (frame_number == DUMP_FRAME)
        write_ppm(pixels, slot.width, slot.height);

    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    check_gl("glUnmapBuffer");

    slot.pending = false;
}

void report_summary()
{
    measurement.summary_reported = true;

    const double average = measurement.total_micros / static_cast<double>(measurement.frame_count);
    log_line("%s: %d frames, average %.1f us, fastest %.1f us, slowest %.1f us", subject_name,
             measurement.frame_count, average, measurement.fastest_micros, measurement.slowest_micros);
    log_line("done — copy Log.txt and spike25_frame.ppm out of the X-Plane folder");
}

void measure_frame(double micros)
{
    if (measurement.frame_count == 0 || micros < measurement.fastest_micros)
        measurement.fastest_micros = micros;
    if (micros > measurement.slowest_micros)
        measurement.slowest_micros = micros;

    measurement.total_micros += micros;
    ++measurement.frame_count;

    if (measurement.frame_count >= MEASURED_FRAMES)
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

    if (measurement.frame_count == 0)
        log_line("%s: viewport %dx%d at origin %d,%d", subject_name, width, height, viewport[0], viewport[1]);

    GLint saved_alignment = 4;
    GLint saved_binding   = 0;
    glGetIntegerv(GL_PACK_ALIGNMENT, &saved_alignment);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &saved_binding);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    start_readback(slots[next_slot], viewport, width, height);

    PixelBufferSlot &ready = slots[1 - next_slot];
    next_slot              = 1 - next_slot;
    consume_ready_slot(ready, measurement.frame_count);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(saved_binding));
    glPixelStorei(GL_PACK_ALIGNMENT, saved_alignment);

    const auto elapsed = std::chrono::steady_clock::now() - started;
    measure_frame(std::chrono::duration<double, std::micro>(elapsed).count());
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

    if (device_id == subject_device)
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
