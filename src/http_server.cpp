/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#include "http_server.hpp"

#include "command_dispatch.hpp"
#include "device_presence.hpp"
#include "device_registry.hpp"
#include "frame_pipeline.hpp"
#include "plugin_log.hpp"
#include "text_screen_source.hpp"

#include <civetweb.h>
#include <json.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace xp_gauge_streamer
{

namespace
{

constexpr char STREAM_URI_PREFIX[]  = "/stream/";
constexpr char SCREEN_URI_PREFIX[]  = "/screen/";
constexpr char DEVICE_URI_PREFIX[]  = "/device/";
constexpr char DEVICES_URI[]        = "/devices";
constexpr char CONTROL_URI[]        = "/control";
constexpr char MULTIPART_BOUNDARY[] = "xpgsframe";

// Every open stream occupies one civetweb thread for as long as the client
// watches, so this is the ceiling on simultaneous viewers plus asset requests.
constexpr char WORKER_THREADS[] = "12";

// How long a stream waits before looking for a new frame. Well below the
// capture interval, so no frame waits noticeably, and idle enough to cost
// nothing while the GNS screen is static.
constexpr auto FRAME_POLL_INTERVAL = std::chrono::milliseconds(10);

// A static MCDU or FMC page sends nothing, and only a write notices a client that has
// gone — the comment line keeps the viewer count honest.
constexpr auto SCREEN_KEEPALIVE_INTERVAL = std::chrono::seconds(5);

mg_context *server = nullptr;

// Kept because /device/<slug> answers from a file rather than civetweb's static
// handler, which never sees that path.
std::string document_root;

// Cleared before mg_stop() so open streams leave their loop instead of holding
// the shutdown until their client disconnects.
std::atomic<bool> streaming_allowed{false};

// Counted apart: a video viewer costs a framebuffer readback, a text viewer a
// round of dataref reads, and each count switches only its own work on.
std::atomic<int> open_streams{0};
std::atomic<int> open_screens{0};

// Keeps a viewer count correct however a request ends — the count decides
// whether the plugin captures at all.
class ViewerRegistration
{
  public:
    ViewerRegistration(std::string what, std::atomic<int> &viewer_count) : name(std::move(what)), count(viewer_count)
    {
        log_format("%s: viewer joined (%d total)", name.c_str(), count.fetch_add(1) + 1);
    }

    ~ViewerRegistration() { log_format("%s: viewer left (%d total)", name.c_str(), count.fetch_sub(1) - 1); }

    ViewerRegistration(const ViewerRegistration &)            = delete;
    ViewerRegistration &operator=(const ViewerRegistration &) = delete;

  private:
    std::string       name;
    std::atomic<int> &count;
};

// /stream/<slug>, /screen/<slug> and /device/<slug> carry the slug the same way.
std::string_view segment_after_prefix(const char *uri, size_t prefix_length)
{
    const std::string_view path(uri != nullptr ? uri : "");
    if (path.size() <= prefix_length)
        return {};

    return path.substr(prefix_length);
}

void send_stream_headers(mg_connection *connection)
{
    mg_printf(connection,
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: multipart/x-mixed-replace; boundary=%s\r\n"
              "Cache-Control: no-store\r\n"
              "Pragma: no-cache\r\n"
              "Connection: close\r\n"
              "\r\n",
              MULTIPART_BOUNDARY);
}

// False once the client is gone — a half-written part is not worth repairing,
// the browser reconnects.
bool send_part(mg_connection *connection, const std::vector<unsigned char> &jpeg)
{
    char      header[128];
    const int header_length = std::snprintf(header, sizeof(header),
                                            "--%s\r\n"
                                            "Content-Type: image/jpeg\r\n"
                                            "Content-Length: %zu\r\n"
                                            "\r\n",
                                            MULTIPART_BOUNDARY, jpeg.size());

    return mg_write(connection, header, static_cast<size_t>(header_length)) > 0 &&
           mg_write(connection, jpeg.data(), jpeg.size()) > 0 && mg_write(connection, "\r\n", 2) > 0;
}

// Always sends the newest frame, never a queued one: a client too slow to keep
// up simply misses frames instead of falling further behind with every one.
void stream_frames(mg_connection *connection, DeviceId device_id)
{
    std::vector<unsigned char> jpeg;
    std::uint64_t              sequence  = 0;
    std::uint64_t              last_sent = 0;

    while (streaming_allowed.load())
    {
        if (!latest_jpeg(device_id, jpeg, sequence) || sequence == last_sent)
        {
            std::this_thread::sleep_for(FRAME_POLL_INTERVAL);
            continue;
        }

        if (!send_part(connection, jpeg))
            return;

        last_sent = sequence;
    }
}

int handle_stream(mg_connection *connection, void *)
{
    const mg_request_info  *request = mg_get_request_info(connection);
    const DeviceDescriptor *device =
        find_device_by_slug(segment_after_prefix(request->local_uri, sizeof(STREAM_URI_PREFIX) - 1));

    if (device == nullptr || !device->enabled || device->source != ScreenSource::framebuffer)
    {
        mg_send_http_error(connection, 404, "%s", "No stream for this device");
        return 404;
    }

    const ViewerRegistration registration(std::string(device->display_name) + " stream", open_streams);

    send_stream_headers(connection);
    stream_frames(connection, device->device_id);
    return 200;
}

void send_event_stream_headers(mg_connection *connection)
{
    mg_printf(connection, "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/event-stream\r\n"
                          "Cache-Control: no-store\r\n"
                          "Connection: close\r\n"
                          "\r\n");
}

// One server-sent event per screen. The JSON never contains a line break, so a
// single data line carries it.
bool send_event(mg_connection *connection, const std::string &json)
{
    return mg_write(connection, "data: ", 6) > 0 && mg_write(connection, json.data(), json.size()) > 0 &&
           mg_write(connection, "\n\n", 2) > 0;
}

bool send_keepalive(mg_connection *connection) { return mg_write(connection, ":\n\n", 3) > 0; }

// Same pattern as stream_frames: only the newest screen, and only when it
// changed. EventSource reconnects by itself when this returns.
void stream_screens(mg_connection *connection, DeviceId device_id)
{
    std::string   json;
    std::uint64_t sequence   = 0;
    std::uint64_t last_sent  = 0;
    auto          last_write = std::chrono::steady_clock::now();

    while (streaming_allowed.load())
    {
        const auto now = std::chrono::steady_clock::now();

        if (latest_text_screen(device_id, json, sequence) && sequence != last_sent)
        {
            if (!send_event(connection, json))
                return;
            last_sent  = sequence;
            last_write = now;
            continue;
        }

        if (now - last_write >= SCREEN_KEEPALIVE_INTERVAL)
        {
            if (!send_keepalive(connection))
                return;
            last_write = now;
        }

        std::this_thread::sleep_for(FRAME_POLL_INTERVAL);
    }
}

int handle_screen(mg_connection *connection, void *)
{
    const mg_request_info  *request = mg_get_request_info(connection);
    const DeviceDescriptor *device =
        find_device_by_slug(segment_after_prefix(request->local_uri, sizeof(SCREEN_URI_PREFIX) - 1));

    if (device == nullptr || !device->enabled || device->source != ScreenSource::text_datarefs)
    {
        mg_send_http_error(connection, 404, "%s", "No screen for this device");
        return 404;
    }

    const ViewerRegistration registration(std::string(device->display_name) + " screen", open_screens);

    send_event_stream_headers(connection);
    stream_screens(connection, device->device_id);
    return 200;
}

// Which units exist depends on the aircraft's panel, and only the sim can answer
// that. The answer comes from the main thread's presence check, so listing the
// devices costs neither capture time nor a wait for the first frame.
int handle_devices(mg_connection *connection, void *)
{
    nlohmann::json devices = nlohmann::json::array();
    for (const DeviceDescriptor &device : all_devices())
    {
        devices.push_back({{"slug", device.slug},
                           {"type", device.type},
                           {"name", device.display_name},
                           {"screen", device.source == ScreenSource::framebuffer ? "mjpeg" : "text"},
                           {"present", device.enabled && device_is_in_aircraft(device.device_id)}});
    }

    const std::string body = nlohmann::json{{"devices", devices}}.dump();
    mg_send_http_ok(connection, "application/json", static_cast<long long>(body.size()));
    mg_write(connection, body.data(), body.size());
    return 200;
}

// /device/<slug> carries the choice in the URL so a tablet can bookmark one
// unit. The page behind every slug is the same file — which bezel it draws is
// the frontend's business, decided from the device's type.
int handle_device_page(mg_connection *connection, void *)
{
    const mg_request_info *request = mg_get_request_info(connection);
    const std::string_view slug    = segment_after_prefix(request->local_uri, sizeof(DEVICE_URI_PREFIX) - 1);

    if (find_device_by_slug(slug) == nullptr)
    {
        mg_send_http_error(connection, 404, "%s", "No such device");
        return 404;
    }

    const std::string page = document_root + "/device.html";
    mg_send_file(connection, page.c_str());
    return 200;
}

std::string read_string(const nlohmann::json &message, const char *key)
{
    const auto entry = message.find(key);
    return entry != message.end() && entry->is_string() ? entry->get<std::string>() : std::string();
}

// Everything here arrives from the network: parse defensively, hand on only
// what command_dispatch recognises, and drop the rest with a log line.
void handle_control_message(const char *data, size_t length)
{
    const nlohmann::json message = nlohmann::json::parse(data, data + length, nullptr, false);
    if (message.is_discarded() || !message.is_object())
    {
        log_format("control message is not JSON, dropped");
        return;
    }

    const std::string device = read_string(message, "device");
    const std::string button = read_string(message, "button");

    if (!press_button(device, button))
        log_format("control message rejected: device=%s button=%s", device.c_str(), button.c_str());
}

int on_websocket_connect(const mg_connection *, void *)
{
    return 0; // 0 accepts the handshake
}

int on_websocket_data(mg_connection *, int bits, char *data, size_t length, void *)
{
    constexpr unsigned OPCODE_MASK  = 0x0f;
    constexpr unsigned OPCODE_TEXT  = 0x1;
    constexpr unsigned OPCODE_CLOSE = 0x8;

    const unsigned opcode = static_cast<unsigned>(bits) & OPCODE_MASK;
    if (opcode == OPCODE_CLOSE)
        return 0;

    if (opcode == OPCODE_TEXT)
        handle_control_message(data, length);

    return 1; // keep the connection open
}

} // namespace

bool start_server(const ServerConfig &config)
{
    if (server != nullptr)
        return true;

    const std::string listening_ports = config.bind_address + ":" + std::to_string(config.port);
    document_root                     = config.document_root;

    const char *options[] = {"listening_ports", listening_ports.c_str(), "document_root", config.document_root.c_str(),
                             "num_threads", WORKER_THREADS, "enable_directory_listing", "no",
                             // The frontend is edited in place and reloaded; an
                             // hour of browser caching would hide every change.
                             "static_file_max_age", "0", nullptr};

    mg_callbacks callbacks = {};

    streaming_allowed.store(true);
    server = mg_start(&callbacks, nullptr, options);
    if (server == nullptr)
    {
        streaming_allowed.store(false);
        log_format("http server failed to start on %s — port in use?", listening_ports.c_str());
        return false;
    }

    mg_set_request_handler(server, STREAM_URI_PREFIX, handle_stream, nullptr);
    mg_set_request_handler(server, SCREEN_URI_PREFIX, handle_screen, nullptr);
    mg_set_request_handler(server, DEVICE_URI_PREFIX, handle_device_page, nullptr);
    mg_set_request_handler(server, DEVICES_URI, handle_devices, nullptr);
    mg_set_websocket_handler(server, CONTROL_URI, on_websocket_connect, nullptr, on_websocket_data, nullptr, nullptr);
    log_format("http server listening on %s, web root %s", listening_ports.c_str(), config.document_root.c_str());
    return true;
}

void stop_server()
{
    if (server == nullptr)
        return;

    streaming_allowed.store(false);
    mg_stop(server);
    server = nullptr;

    log_format("http server stopped");
}

int active_stream_count() { return open_streams.load(); }

int active_screen_count() { return open_screens.load(); }

bool server_is_running() { return server != nullptr; }

} // namespace xp_gauge_streamer
