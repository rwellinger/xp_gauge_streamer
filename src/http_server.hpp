/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#pragma once

#include <string>

namespace xp_gauge_streamer
{

struct ServerConfig
{
    std::string bind_address;
    int         port;
    std::string document_root;
};

// Starts the embedded HTTP server: static assets from `document_root` plus one
// MJPEG endpoint per device under /stream/<slug>. False when the port is taken.
bool start_server(const ServerConfig &config);

// Ends every open stream and joins civetweb's threads. A civetweb thread still
// running at plugin unload takes X-Plane down with it.
void stop_server();

// How many MJPEG streams are being served right now. Safe from any thread.
int active_stream_count();

// False when the server never came up — a taken port is the usual reason.
bool server_is_running();

} // namespace xp_gauge_streamer
