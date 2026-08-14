#include "avionics_capture.hpp"

#include "capture_schedule.hpp"
#include "plugin_log.hpp"

#include <XPLM/XPLMDisplay.h>
#include <XPLM/XPLMProcessing.h>

#include <OpenGL/gl.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace xp_gauge_streamer
{

namespace
{

// The GNS screen redraws far slower than the sim renders; a readback per frame
// would cost main-thread time for identical pixels. Even with the asynchronous
// readback below, every capture costs a synchronisation with X-Plane's Metal
// renderer, so the rate is the main lever on what this plugin costs.
constexpr double CAPTURE_FRAMES_PER_SECOND = 6.0;

// Legacy <OpenGL/gl.h> is GL 2.1 and lacks these names; the values are stable.
#ifndef GL_PIXEL_PACK_BUFFER
constexpr GLenum GL_PIXEL_PACK_BUFFER = 0x88EB;
#endif
#ifndef GL_PIXEL_PACK_BUFFER_BINDING
constexpr GLenum GL_PIXEL_PACK_BUFFER_BINDING = 0x88ED;
#endif
#ifndef GL_STREAM_READ
constexpr GLenum GL_STREAM_READ = 0x88E1;
#endif
#ifndef GL_READ_ONLY
constexpr GLenum GL_READ_ONLY = 0x88B8;
#endif

struct PixelBuffer
{
    GLuint name    = 0;
    size_t bytes   = 0;
    int    width   = 0;
    int    height  = 0;
    bool   pending = false;
};

// Two buffers per device: glReadPixels fills one without waiting while the
// other — filled one capture ago and long since transferred — is mapped and
// read. That is what keeps the readback off the critical path.
struct DeviceBuffers
{
    DeviceId    device_id;
    PixelBuffer slots[2];
    int         next = 0;
};

struct CaptureTarget
{
    DeviceId       device_id;
    std::string    display_name;
    XPLMAvionicsID handle       = nullptr;
    bool           logged_first = false;
};

FrameSink       frame_sink = nullptr;
CaptureSchedule schedule(CAPTURE_FRAMES_PER_SECOND);
bool            readback_enabled = false;

// One bit per device id, so a civetweb thread can answer "does this aircraft
// have the unit?" without touching the XPLM API — only the flight loop may.
std::atomic<unsigned> devices_in_aircraft{0};

// XPLMDeviceID values are a small enumeration; anything beyond the mask's width
// would need a different carrier than a single lock-free word.
constexpr DeviceId HIGHEST_MAPPED_DEVICE_ID = 31;

// Held by pointer because each target's address is handed to X-Plane as the
// callback refcon — a reallocating vector of values would dangle.
std::vector<std::unique_ptr<CaptureTarget>> targets;

// Outlives registration: GL objects may only be deleted on the thread and
// context that created them, and neither stop_capture() nor a device toggle
// runs inside the draw callback. Keeping the buffers lets a re-enabled device
// reuse them instead of leaking a fresh pair.
std::vector<std::unique_ptr<DeviceBuffers>> device_buffers;

CaptureTarget *find_target(DeviceId device_id)
{
    for (const std::unique_ptr<CaptureTarget> &target : targets)
    {
        if (target->device_id == device_id)
            return target.get();
    }
    return nullptr;
}

DeviceBuffers &buffers_for(DeviceId device_id)
{
    for (const std::unique_ptr<DeviceBuffers> &buffers : device_buffers)
    {
        if (buffers->device_id == device_id)
            return *buffers;
    }

    device_buffers.push_back(std::make_unique<DeviceBuffers>());
    device_buffers.back()->device_id = device_id;
    return *device_buffers.back();
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

void start_readback(PixelBuffer &slot, const GLint *viewport, int width, int height)
{
    const size_t needed_bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 3;

    if (slot.name == 0)
        glGenBuffers(1, &slot.name);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, slot.name);

    if (slot.bytes != needed_bytes)
    {
        glBufferData(GL_PIXEL_PACK_BUFFER, static_cast<GLsizeiptr>(needed_bytes), nullptr, GL_STREAM_READ);
        slot.bytes   = needed_bytes;
        slot.pending = false;
    }

    // Reading into a bound pack buffer returns immediately; the transfer
    // completes in the background.
    glReadPixels(viewport[0], viewport[1], width, height, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

    slot.width   = width;
    slot.height  = height;
    slot.pending = true;
}

void deliver_ready_frame(CaptureTarget &target, PixelBuffer &slot)
{
    if (!slot.pending)
        return;

    glBindBuffer(GL_PIXEL_PACK_BUFFER, slot.name);
    const auto *pixels = static_cast<const unsigned char *>(glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY));

    if (pixels != nullptr)
    {
        frame_sink(target.device_id, pixels, slot.width, slot.height);
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }

    slot.pending = false;
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

    // "The OpenGL state will be unknown" per the SDK header — save what we set.
    GLint saved_alignment = 4;
    GLint saved_binding   = 0;
    glGetIntegerv(GL_PACK_ALIGNMENT, &saved_alignment);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &saved_binding);
    // Without this a width not divisible by 4 gets row padding and a skewed image.
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    DeviceBuffers &buffers = buffers_for(target.device_id);
    start_readback(buffers.slots[buffers.next], viewport, width, height);

    PixelBuffer &ready = buffers.slots[1 - buffers.next];
    buffers.next       = 1 - buffers.next;

    if (!target.logged_first)
        log_first_frame(target, width, height);

    deliver_ready_frame(target, ready);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(saved_binding));
    glPixelStorei(GL_PACK_ALIGNMENT, saved_alignment);
}

int draw_after(XPLMDeviceID device_id, int is_before, void *refcon)
{
    auto *target = static_cast<CaptureTarget *>(refcon);

    if (frame_sink != nullptr && readback_enabled && schedule.is_due(target->device_id, XPLMGetElapsedTime()))
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

    refresh_device_presence();
}

void set_readback_enabled(bool enabled)
{
    if (enabled == readback_enabled)
        return;

    readback_enabled = enabled;

    // Forget the timestamps so a re-enabled device captures at once instead of
    // waiting out an interval that elapsed while nobody was watching.
    if (enabled)
    {
        for (const std::unique_ptr<CaptureTarget> &target : targets)
            schedule.forget(target->device_id);
    }
}

void refresh_device_presence()
{
    unsigned present = 0;
    for (const std::unique_ptr<CaptureTarget> &target : targets)
    {
        if (target->device_id > HIGHEST_MAPPED_DEVICE_ID || XPLMIsAvionicsBound(target->handle) == 0)
            continue;

        present |= 1u << static_cast<unsigned>(target->device_id);
    }

    devices_in_aircraft.store(present, std::memory_order_relaxed);
}

bool device_is_in_aircraft(DeviceId device_id)
{
    if (device_id < 0 || device_id > HIGHEST_MAPPED_DEVICE_ID)
        return false;

    return (devices_in_aircraft.load(std::memory_order_relaxed) & (1u << static_cast<unsigned>(device_id))) != 0;
}

void stop_capture()
{
    for (const std::unique_ptr<CaptureTarget> &target : targets)
    {
        XPLMUnregisterAvionicsCallbacks(target->handle);
        schedule.forget(target->device_id);
    }
    targets.clear();
    frame_sink       = nullptr;
    readback_enabled = false;
    devices_in_aircraft.store(0, std::memory_order_relaxed);
}

} // namespace xp_gauge_streamer
