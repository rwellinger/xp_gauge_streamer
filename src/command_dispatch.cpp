/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#include "command_dispatch.hpp"

#include "command_catalog.hpp"
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

// Resolved on first use, on the main thread: an aircraft's own commands (the
// ToLiss MCDU keys) only exist once that aircraft has loaded, long after the
// plugin started. Only the flight loop touches the cache, so it needs no lock.
std::map<std::string, XPLMCommandRef> resolved_commands;

std::vector<std::string> queued_presses;
std::mutex               dispatch_mutex;
bool                     running = false;

XPLMCommandRef resolve_command(const std::string &name)
{
    const auto cached = resolved_commands.find(name);
    if (cached != resolved_commands.end())
        return cached->second;

    XPLMCommandRef command = XPLMFindCommand(name.c_str());
    if (command == nullptr)
    {
        log_format("command not found: %s", name.c_str());
        return nullptr;
    }

    resolved_commands.emplace(name, command);
    return command;
}

float run_queued_presses(float, float, int, void *)
{
    std::vector<std::string> presses;
    {
        const std::lock_guard<std::mutex> lock(dispatch_mutex);
        presses.swap(queued_presses);
    }

    // Outside the lock: a civetweb thread must never wait on the sim.
    for (const std::string &name : presses)
    {
        XPLMCommandRef command = resolve_command(name);
        if (command != nullptr)
            XPLMCommandOnce(command);
    }

    return DRAIN_EVERY_FRAME;
}

} // namespace

void start_dispatch()
{
    const std::lock_guard<std::mutex> lock(dispatch_mutex);
    if (running)
        return;

    running = true;

    XPLMRegisterFlightLoopCallback(run_queued_presses, DRAIN_EVERY_FRAME, nullptr);
    log_format("command dispatch ready");
}

void stop_dispatch()
{
    XPLMUnregisterFlightLoopCallback(run_queued_presses, nullptr);

    const std::lock_guard<std::mutex> lock(dispatch_mutex);
    running = false;
    queued_presses.clear();
    resolved_commands.clear();
}

bool press_button(std::string_view device_slug, std::string_view button)
{
    const std::string name = command_name(device_slug, button);
    if (name.empty())
        return false;

    const std::lock_guard<std::mutex> lock(dispatch_mutex);
    if (!running)
        return false;

    if (queued_presses.size() >= QUEUE_LIMIT)
    {
        log_format("press queue full, dropping %s", name.c_str());
        return false;
    }

    queued_presses.push_back(name);
    return true;
}

} // namespace xp_gauge_streamer
