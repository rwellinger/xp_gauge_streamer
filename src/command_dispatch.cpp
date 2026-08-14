#include "command_dispatch.hpp"

#include "command_catalog.hpp"
#include "device_registry.hpp"
#include "plugin_log.hpp"

#include <XPLM/XPLMProcessing.h>
#include <XPLM/XPLMUtilities.h>

#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace xp_gauge_streamer
{

namespace
{

// A click cannot take effect sooner than the next frame anyway, and draining a
// handful of presses costs nothing.
constexpr float DRAIN_EVERY_FRAME = -1.0f;

// A paused or loading sim runs no flight loops while clicks keep arriving.
// Dropping the excess keeps a stuck queue from replaying minutes of input at
// once when the sim resumes.
constexpr size_t QUEUE_LIMIT = 128;

// Resolved once at startup: XPLMFindCommand per keypress would be wasteful,
// and calling it from a civetweb thread would not be safe.
std::map<std::string, XPLMCommandRef> commands;

std::vector<XPLMCommandRef> queued_presses;
std::mutex                  dispatch_mutex;
bool                        running = false;

void resolve_commands()
{
    for (const DeviceDescriptor &device : all_devices())
    {
        for (const std::string_view button : known_buttons())
        {
            const std::string name = command_name(device.slug, button);
            if (commands.find(name) != commands.end())
                continue;

            XPLMCommandRef command = XPLMFindCommand(name.c_str());
            if (command == nullptr)
            {
                log_format("command not found: %s", name.c_str());
                continue;
            }

            commands.emplace(name, command);
        }
    }
}

float run_queued_presses(float, float, int, void *)
{
    std::vector<XPLMCommandRef> presses;
    {
        const std::lock_guard<std::mutex> lock(dispatch_mutex);
        presses.swap(queued_presses);
    }

    // Outside the lock: a civetweb thread must never wait on the sim.
    for (XPLMCommandRef command : presses)
        XPLMCommandOnce(command);

    return DRAIN_EVERY_FRAME;
}

} // namespace

void start_dispatch()
{
    const std::lock_guard<std::mutex> lock(dispatch_mutex);
    if (running)
        return;

    resolve_commands();
    running = true;

    XPLMRegisterFlightLoopCallback(run_queued_presses, DRAIN_EVERY_FRAME, nullptr);
    log_format("command dispatch ready, %zu commands resolved", commands.size());
}

void stop_dispatch()
{
    XPLMUnregisterFlightLoopCallback(run_queued_presses, nullptr);

    const std::lock_guard<std::mutex> lock(dispatch_mutex);
    running = false;
    queued_presses.clear();
    commands.clear();
}

bool press_button(std::string_view device_slug, std::string_view button)
{
    const std::string name = command_name(device_slug, button);
    if (name.empty())
        return false;

    const std::lock_guard<std::mutex> lock(dispatch_mutex);
    if (!running)
        return false;

    const auto command = commands.find(name);
    if (command == commands.end())
        return false;

    if (queued_presses.size() >= QUEUE_LIMIT)
    {
        log_format("press queue full, dropping %s", name.c_str());
        return false;
    }

    queued_presses.push_back(command->second);
    return true;
}

} // namespace xp_gauge_streamer
