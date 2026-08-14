// Phase 0 capture spike (issue #2). Deliberately throwaway: it answers whether
// glReadPixels returns real pixels inside an avionics draw callback while
// X-Plane renders through Metal. Replaced by avionics_capture in phase 2.

#include <XPLM/XPLMDisplay.h>
#include <XPLM/XPLMPlugin.h>
#include <XPLM/XPLMUtilities.h>
#include <civetweb.h>
#include <turbojpeg.h>

#include <OpenGL/gl.h>

#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace
{

constexpr char PLUGIN_NAME[]      = "xp_gauge_streamer";
constexpr char PLUGIN_SIGNATURE[] = "ch.thwelly.xp_gauge_streamer";
constexpr char PLUGIN_DESCRIPTION[] =
    "Streams the GNS430/530 display to a web frontend and forwards clicks back into the sim.";

// X-Plane's plugin API hands out fixed 256-byte buffers for name/signature/description.
constexpr size_t XPLM_STRING_BUFFER_SIZE = 256;

// The device is not necessarily done initialising on its first drawn frame, so
// the spike grabs a slightly later one.
constexpr long CAPTURE_AT_CALL   = 30;
constexpr long PROGRESS_INTERVAL = 300;

struct SpikeDevice
{
    XPLMDeviceID   device_id;
    const char    *name;
    XPLMAvionicsID handle    = nullptr;
    long           callCount = 0;
    bool           captured  = false;
};

std::array<SpikeDevice, 4> spike_devices = {{
    {xplm_device_GNS430_1, "GNS430_1"},
    {xplm_device_GNS430_2, "GNS430_2"},
    {xplm_device_GNS530_1, "GNS530_1"},
    {xplm_device_GNS530_2, "GNS530_2"},
}};

void log_line(const char *message)
{
    char line[512];
    std::snprintf(line, sizeof(line), "[%s] %s\n", PLUGIN_NAME, message);
    XPLMDebugString(line);
}

__attribute__((format(printf, 1, 2))) void log_format(const char *format, ...)
{
    char    message[448];
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    log_line(message);
}

void log_dependency_versions()
{
    log_format("version %s, civetweb %s, libjpeg-turbo %d.%d.%d", XP_GAUGE_STREAMER_VERSION, mg_version(),
               TURBOJPEG_VERSION_NUMBER / 1000000, (TURBOJPEG_VERSION_NUMBER / 1000) % 1000,
               TURBOJPEG_VERSION_NUMBER % 1000);
}

std::string output_path(const char *filename)
{
    char system_path[512] = {0};
    XPLMGetSystemPath(system_path);
    return std::string(system_path) + "Output/" + filename;
}

bool write_jpeg(const std::string &path, const unsigned char *rgb, int width, int height)
{
    tjhandle compressor = tj3Init(TJINIT_COMPRESS);
    if (compressor == nullptr)
        return false;

    tj3Set(compressor, TJPARAM_QUALITY, 90);
    tj3Set(compressor, TJPARAM_SUBSAMP, TJSAMP_444);
    // OpenGL's origin is bottom-left, so let turbojpeg flip instead of us.
    tj3Set(compressor, TJPARAM_BOTTOMUP, 1);

    unsigned char *jpeg      = nullptr;
    size_t         jpeg_size = 0;
    bool           ok        = tj3Compress8(compressor, rgb, width, 0, height, TJPF_RGB, &jpeg, &jpeg_size) == 0;

    if (!ok)
        log_format("JPEG compression failed: %s", tj3GetErrorStr(compressor));

    if (ok)
    {
        std::ofstream file(path, std::ios::binary);
        ok =
            file.is_open() && file.write(reinterpret_cast<const char *>(jpeg), static_cast<std::streamsize>(jpeg_size));
        if (ok)
            log_format("wrote %zu bytes to %s", jpeg_size, path.c_str());
        else
            log_format("could not write %s", path.c_str());
    }

    tj3Free(jpeg);
    tj3Destroy(compressor);
    return ok;
}

size_t count_non_zero(const std::vector<unsigned char> &pixels)
{
    size_t non_zero = 0;
    for (unsigned char value : pixels)
    {
        if (value != 0)
            ++non_zero;
    }
    return non_zero;
}

void capture_once(SpikeDevice &device)
{
    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    log_format("%s: viewport x=%d y=%d w=%d h=%d", device.name, viewport[0], viewport[1], viewport[2], viewport[3]);

    const int width  = viewport[2];
    const int height = viewport[3];
    if (width <= 0 || height <= 0)
    {
        log_format("%s: unusable viewport, no capture", device.name);
        return;
    }

    std::vector<unsigned char> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 3);

    GLint saved_alignment = 4;
    glGetIntegerv(GL_PACK_ALIGNMENT, &saved_alignment);
    // Without this a width not divisible by 4 gets row padding and a skewed image.
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(viewport[0], viewport[1], width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    const GLenum error = glGetError();
    glPixelStorei(GL_PACK_ALIGNMENT, saved_alignment);

    const size_t non_zero = count_non_zero(pixels);
    log_format("%s: glReadPixels error=0x%04x, %zu of %zu bytes non-zero", device.name, error, non_zero, pixels.size());

    if (error != GL_NO_ERROR || non_zero == 0)
    {
        log_format("%s: no usable pixels — see issue #2", device.name);
        return;
    }

    write_jpeg(output_path((std::string("xp_gauge_streamer_spike_") + device.name + ".jpg").c_str()), pixels.data(),
               width, height);
}

int draw_after(XPLMDeviceID device_id, int is_before, void *refcon)
{
    auto *device = static_cast<SpikeDevice *>(refcon);
    ++device->callCount;

    if (!device->captured && device->callCount >= CAPTURE_AT_CALL)
    {
        device->captured = true;
        capture_once(*device);
    }
    else if (device->callCount % PROGRESS_INTERVAL == 0)
    {
        log_format("%s: %ld draw callbacks so far", device->name, device->callCount);
    }

    return 1;
}

void register_devices()
{
    for (SpikeDevice &device : spike_devices)
    {
        XPLMCustomizeAvionics_t params = {};
        params.structSize              = sizeof(XPLMCustomizeAvionics_t);
        params.deviceId                = device.device_id;
        params.drawCallbackAfter       = draw_after;
        params.refcon                  = &device;

        device.handle = XPLMRegisterAvionicsCallbacksEx(&params);
        log_format("%s: register %s", device.name, device.handle != nullptr ? "ok" : "FAILED");
    }
}

void unregister_devices()
{
    for (SpikeDevice &device : spike_devices)
    {
        if (device.handle != nullptr)
        {
            XPLMUnregisterAvionicsCallbacks(device.handle);
            device.handle = nullptr;
        }
        device.callCount = 0;
        device.captured  = false;
    }
}

void log_bound_devices()
{
    for (const SpikeDevice &device : spike_devices)
    {
        if (device.handle == nullptr)
            continue;
        log_format("%s: bound to current aircraft: %s", device.name,
                   XPLMIsAvionicsBound(device.handle) != 0 ? "yes" : "no");
    }
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

PLUGIN_API int XPluginEnable(void)
{
    register_devices();
    return 1;
}

PLUGIN_API void XPluginDisable(void) { unregister_devices(); }

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID from, int message, void *param)
{
    if (message == XPLM_MSG_PLANE_LOADED && reinterpret_cast<intptr_t>(param) == 0)
        log_bound_devices();
}
