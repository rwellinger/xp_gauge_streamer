/*
 * xp_gauge_streamer - GNS430/530 display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#include "plugin_paths.hpp"

#include "plugin_log.hpp"

#include <XPLM/XPLMPlugin.h>
#include <XPLM/XPLMUtilities.h>

#include <filesystem>

namespace xp_gauge_streamer
{

namespace
{

// The SDK writes into caller-provided buffers of a fixed 256 bytes.
constexpr size_t XPLM_PATH_BUFFER_SIZE = 256;

constexpr char SETTINGS_DIRECTORY[] = "xp_gauge_streamer";
constexpr char SETTINGS_FILE_NAME[] = "settings.cfg";

// .../xp_gauge_streamer/mac_x64/xp_gauge_streamer.xpl → .../xp_gauge_streamer
std::filesystem::path plugin_directory()
{
    char file_path[XPLM_PATH_BUFFER_SIZE] = {};
    XPLMGetPluginInfo(XPLMGetMyID(), nullptr, file_path, nullptr, nullptr);

    return std::filesystem::path(file_path).parent_path().parent_path();
}

std::filesystem::path x_plane_root()
{
    char system_path[XPLM_PATH_BUFFER_SIZE] = {};
    XPLMGetSystemPath(system_path);

    return std::filesystem::path(system_path);
}

} // namespace

std::string web_root_path() { return (plugin_directory() / "web").string(); }

std::string settings_file_path()
{
    const std::filesystem::path directory = x_plane_root() / "Output" / SETTINGS_DIRECTORY;

    std::error_code failure;
    std::filesystem::create_directories(directory, failure);
    if (failure)
        log_format("could not create %s: %s", directory.c_str(), failure.message().c_str());

    return (directory / SETTINGS_FILE_NAME).string();
}

} // namespace xp_gauge_streamer
