#include "avionics_capture.hpp"

#include "capture_schedule.hpp"
#include "plugin_log.hpp"

#include <XPLM/XPLMDisplay.h>
#include <XPLM/XPLMProcessing.h>

#include <OpenGL/gl.h>

#include <memory>
#include <string>
#include <vector>

namespace xp_gauge_streamer
{

namespace
{

// The GNS screen redraws far slower than the sim renders; a readback per frame
// would cost main-thread time for identical pixels.
constexpr double CAPTURE_FRAMES_PER_SECOND = 12.0;

struct CaptureTarget
{
    DeviceId                   device_id;
    std::string                display_name;
    XPLMAvionicsID             handle       = nullptr;
    bool                       logged_first = false;
    std::vector<unsigned char> pixels;
};

FrameSink       frame_sink = nullptr;
CaptureSchedule schedule(CAPTURE_FRAMES_PER_SECOND);

// Held by pointer because each target's address is handed to X-Plane as the
// callback refcon — a reallocating vector of values would dangle.
std::vector<std::unique_ptr<CaptureTarget>> targets;

CaptureTarget *find_target(DeviceId device_id)
{
    for (const std::unique_ptr<CaptureTarget> &target : targets)
    {
        if (target->device_id == device_id)
            return target.get();
    }
    return nullptr;
}

// Reports the readback dimensions once per device. GL_INVALID_OPERATION here
// means the device's framebuffer is multisampled and needs a resolve blit —
// not the case on any configuration measured so far (see issue #2).
void log_first_frame(CaptureTarget &target, int width, int height)
{
    target.logged_first = true;
    const GLenum error  = glGetError();
    log_format("%s: capturing %dx%d at %.0f fps%s", target.display_name.c_str(), width, height,
               CAPTURE_FRAMES_PER_SECOND, error != GL_NO_ERROR ? " — glReadPixels FAILED" : "");
}

void capture_frame(CaptureTarget &target)
{
    // The viewport is read every time: X-Plane may resize the device's
    // framebuffer at runtime.
    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);

    const int width  = viewport[2];
    const int height = viewport[3];
    if (width <= 0 || height <= 0)
        return;

    target.pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 3);

    // "The OpenGL state will be unknown" per the SDK header — save what we set.
    GLint saved_alignment = 4;
    glGetIntegerv(GL_PACK_ALIGNMENT, &saved_alignment);
    // Without this a width not divisible by 4 gets row padding and a skewed image.
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(viewport[0], viewport[1], width, height, GL_RGB, GL_UNSIGNED_BYTE, target.pixels.data());
    glPixelStorei(GL_PACK_ALIGNMENT, saved_alignment);

    // No glFinish and no per-frame glGetError: both stall the pipeline, and the
    // device has already finished drawing by the time the after-callback runs.
    if (!target.logged_first)
        log_first_frame(target, width, height);

    frame_sink(target.device_id, target.pixels.data(), width, height);
}

int draw_after(XPLMDeviceID device_id, int is_before, void *refcon)
{
    auto *target = static_cast<CaptureTarget *>(refcon);

    if (frame_sink != nullptr && schedule.is_due(target->device_id, XPLMGetElapsedTime()))
        capture_frame(*target);

    return 1;
}

void register_target(const DeviceDescriptor &descriptor)
{
    auto target          = std::make_unique<CaptureTarget>();
    target->device_id    = descriptor.device_id;
    target->display_name = std::string(descriptor.display_name);

    XPLMCustomizeAvionics_t params = {};
    params.structSize              = sizeof(XPLMCustomizeAvionics_t);
    params.deviceId                = static_cast<XPLMDeviceID>(descriptor.device_id);
    params.drawCallbackAfter       = draw_after;
    params.refcon                  = target.get();

    target->handle = XPLMRegisterAvionicsCallbacksEx(&params);
    if (target->handle == nullptr)
    {
        log_format("%s: registration FAILED", target->display_name.c_str());
        return;
    }

    log_format("%s: registered", target->display_name.c_str());
    targets.push_back(std::move(target));
}

void unregister_target(DeviceId device_id)
{
    for (auto entry = targets.begin(); entry != targets.end(); ++entry)
    {
        if ((*entry)->device_id != device_id)
            continue;

        XPLMUnregisterAvionicsCallbacks((*entry)->handle);
        log_format("%s: unregistered", (*entry)->display_name.c_str());
        targets.erase(entry);
        schedule.forget(device_id);
        return;
    }
}

} // namespace

void start_capture(FrameSink sink)
{
    frame_sink = sink;
    refresh_capture_registrations();
}

void refresh_capture_registrations()
{
    for (const DeviceDescriptor &descriptor : all_devices())
    {
        const bool is_registered = find_target(descriptor.device_id) != nullptr;

        if (descriptor.enabled && !is_registered)
            register_target(descriptor);
        else if (!descriptor.enabled && is_registered)
            unregister_target(descriptor.device_id);
    }
}

void stop_capture()
{
    for (const std::unique_ptr<CaptureTarget> &target : targets)
    {
        XPLMUnregisterAvionicsCallbacks(target->handle);
        schedule.forget(target->device_id);
    }
    targets.clear();
    frame_sink = nullptr;
}

} // namespace xp_gauge_streamer
