/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#include "text_screen_source.hpp"

#include "device_presence.hpp"
#include "plugin_log.hpp"
#include "text_screen_formats.hpp"

#include <XPLM/XPLMDataAccess.h>
#include <XPLM/XPLMProcessing.h>

#include <memory>
#include <mutex>
#include <vector>

namespace xp_gauge_streamer
{

namespace
{

// An aircraft change is noticed within a second — as quick as the framebuffer
// devices' presence check.
constexpr float PRESENCE_INTERVAL_SECONDS = 1.0f;

// Keys typed on a tablet should echo in the scratchpad without a noticeable
// lag. Reading one unit is at most 150 short dataref copies (ToLiss; the Zibo
// needs 57).
constexpr float READ_INTERVAL_SECONDS = 0.1f;

// A layer holds 24 columns and a NUL; the slack costs nothing.
constexpr int LAYER_BUFFER_SIZE = 64;

struct Unit
{
    DeviceId            device_id;
    std::string         dataref_prefix;
    std::string         display_name;
    const ScreenFormat *format;

    // Parallel to format->layers; empty while the aircraft offers none.
    // Main thread only.
    std::vector<XPLMDataRef> layers;
    bool                     present = false;

    // Guarded by screens_mutex.
    std::string   screen_json;
    std::uint64_t sequence = 0;
};

// Only the main thread changes the vector, and only under screens_mutex, so it
// may walk it without the lock; the server's threads look units up with it.
std::vector<std::unique_ptr<Unit>> units;
std::mutex                         screens_mutex;

bool reading_enabled = false;

// A plugin's datarefs disappear with the aircraft that registered them and
// reappear under new handles with the next one. The handles are looked up
// again whenever the first one has gone stale.
bool bind_layers(Unit &unit)
{
    if (!unit.layers.empty() && XPLMIsDataRefGood(unit.layers.front()) != 0)
        return true;

    unit.layers.clear();
    for (const ScreenLayer &layer : unit.format->layers)
    {
        XPLMDataRef dataref = XPLMFindDataRef((unit.dataref_prefix + layer.suffix).c_str());
        if (dataref == nullptr)
        {
            unit.layers.clear();
            return false;
        }
        unit.layers.push_back(dataref);
    }

    return XPLMIsDataRefGood(unit.layers.front()) != 0;
}

std::string read_screen(const Unit &unit)
{
    const std::vector<ScreenLayer> &layers = unit.format->layers;
    TextScreen                      screen(*unit.format);
    char                            buffer[LAYER_BUFFER_SIZE];

    for (size_t index = 0; index < layers.size(); ++index)
    {
        const int length = XPLMGetDatab(unit.layers[index], buffer, 0, LAYER_BUFFER_SIZE);
        if (length > 0)
            screen.apply(layers[index], std::string_view(buffer, static_cast<size_t>(length)));
    }

    return screen.to_json();
}

void store_screen(Unit &unit, std::string json)
{
    const std::lock_guard<std::mutex> lock(screens_mutex);
    if (json == unit.screen_json)
        return;

    unit.screen_json = std::move(json);
    ++unit.sequence;
}

void update_presence(Unit &unit)
{
    const DeviceDescriptor *device  = find_device_by_id(unit.device_id);
    const bool              present = device != nullptr && device->enabled && bind_layers(unit);

    if (present != unit.present)
        log_format("%s: %s", unit.display_name.c_str(), present ? "in this aircraft" : "not in this aircraft");

    unit.present = present;
    set_device_present(unit.device_id, present);
}

float watch_units(float, float, int, void *)
{
    for (const std::unique_ptr<Unit> &unit : units)
    {
        update_presence(*unit);
        if (unit->present && reading_enabled)
            store_screen(*unit, read_screen(*unit));
    }

    return reading_enabled ? READ_INTERVAL_SECONDS : PRESENCE_INTERVAL_SECONDS;
}

Unit *find_unit(DeviceId device_id)
{
    for (const std::unique_ptr<Unit> &unit : units)
    {
        if (unit->device_id == device_id)
            return unit.get();
    }
    return nullptr;
}

} // namespace

void start_text_screen_source()
{
    if (!units.empty())
        return;

    for (const DeviceDescriptor &device : all_devices())
    {
        const ScreenFormat *format = screen_format_for(device.type);
        if (device.source != ScreenSource::text_datarefs || format == nullptr)
            continue;

        auto unit            = std::make_unique<Unit>();
        unit->device_id      = device.device_id;
        unit->dataref_prefix = std::string(device.dataref_prefix);
        unit->display_name   = std::string(device.display_name);
        unit->format         = format;
        const std::lock_guard<std::mutex> lock(screens_mutex);
        units.push_back(std::move(unit));
    }

    XPLMRegisterFlightLoopCallback(watch_units, PRESENCE_INTERVAL_SECONDS, nullptr);
}

void stop_text_screen_source()
{
    XPLMUnregisterFlightLoopCallback(watch_units, nullptr);

    for (const std::unique_ptr<Unit> &unit : units)
        set_device_present(unit->device_id, false);

    const std::lock_guard<std::mutex> lock(screens_mutex);
    units.clear();
    reading_enabled = false;
}

void set_text_screen_reading_enabled(bool enabled)
{
    if (enabled == reading_enabled)
        return;

    reading_enabled = enabled;
    log_format("text screen reading %s", enabled ? "started" : "stopped");

    // Waiting out the presence interval would leave a new viewer staring at
    // an empty screen for up to a second.
    if (enabled)
        XPLMSetFlightLoopCallbackInterval(watch_units, READ_INTERVAL_SECONDS, 1, nullptr);
}

bool latest_text_screen(DeviceId device_id, std::string &json, std::uint64_t &sequence)
{
    const std::lock_guard<std::mutex> lock(screens_mutex);

    const Unit *unit = find_unit(device_id);
    if (unit == nullptr || unit->screen_json.empty())
        return false;

    json     = unit->screen_json;
    sequence = unit->sequence;
    return true;
}

} // namespace xp_gauge_streamer
