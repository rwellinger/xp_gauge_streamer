/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#include "plugin_ui.hpp"

#include "device_presence.hpp"
#include "device_registry.hpp"
#include "http_server.hpp"
#include "network_info.hpp"
#include "opengl.hpp"
#include "settings.hpp"

#include <XPLM/XPLMDisplay.h>
#include <XPLM/XPLMGraphics.h>
#include <XPLM/XPLMMenus.h>
#include <XPLM/XPLMProcessing.h>
#include <XPLM/XPLMUtilities.h>

#include <backends/imgui_impl_opengl2.h>
#include <imgui.h>

#include <algorithm>
#include <string>

namespace xp_gauge_streamer
{

namespace
{

constexpr float WINDOW_WIDTH  = 540.0f;
constexpr float WINDOW_HEIGHT = 430.0f;

// The URL is the reason this window exists — it gets its own oversized line.
constexpr float URL_FONT_SCALE = 1.6f;

// ImGui carries a clipboard implementation for macOS and Win32 and falls back
// to a process-local buffer everywhere else, where copying would only look
// like it worked.
#if defined(__linux__)
constexpr bool CLIPBOARD_IS_REAL = false;
#else
constexpr bool CLIPBOARD_IS_REAL = true;
#endif

// The default ImGui font only carries glyphs up to U+00FF, so window text stays
// within Latin-1 — an em dash would render as a replacement character.
constexpr char PRODUCT_NAME[] = "Welly's Gauge Streamer";

ImGuiContext  *imgui_context   = nullptr;
XPLMWindowID   input_window    = nullptr;
XPLMMenuID     plugin_menu     = nullptr;
DevicesChanged devices_changed = nullptr;

bool   window_open     = false;
double last_frame_time = 0.0;
int    port_field      = 0;
bool   port_stored     = false;

// ── The X-Plane window underneath ────────────────────────────────────────────
// A full-screen, undecorated window that only collects mouse and keyboard
// input; ImGui draws its own chrome on top.

int forward_mouse(XPLMWindowID window, int x, int y, XPLMMouseStatus status, void *)
{
    int left = 0, top = 0, right = 0, bottom = 0;
    XPLMGetWindowGeometry(window, &left, &top, &right, &bottom);

    ImGuiIO &io = ImGui::GetIO();
    io.AddMousePosEvent(static_cast<float>(x - left), static_cast<float>(top - y));

    if (status == xplm_MouseDown)
        io.AddMouseButtonEvent(0, true);
    if (status == xplm_MouseUp)
        io.AddMouseButtonEvent(0, false);

    return 1;
}

int forward_scroll(XPLMWindowID window, int x, int y, int, int clicks, void *)
{
    int left = 0, top = 0, right = 0, bottom = 0;
    XPLMGetWindowGeometry(window, &left, &top, &right, &bottom);

    ImGuiIO &io = ImGui::GetIO();
    io.AddMousePosEvent(static_cast<float>(x - left), static_cast<float>(top - y));
    io.AddMouseWheelEvent(0.0f, static_cast<float>(clicks));

    return 1;
}

XPLMCursorStatus forward_cursor(XPLMWindowID window, int x, int y, void *)
{
    int left = 0, top = 0, right = 0, bottom = 0;
    XPLMGetWindowGeometry(window, &left, &top, &right, &bottom);

    ImGui::GetIO().AddMousePosEvent(static_cast<float>(x - left), static_cast<float>(top - y));
    return xplm_CursorDefault;
}

void close_window();

void forward_key(XPLMWindowID, char key, XPLMKeyFlags flags, char virtual_key, void *, int losing_focus)
{
    const bool key_down = (static_cast<unsigned>(flags) & static_cast<unsigned>(xplm_DownFlag)) != 0;
    if (losing_focus != 0 || !key_down)
        return;

    ImGuiIO &io = ImGui::GetIO();

    if (key >= 32 && key < 127)
        io.AddInputCharacter(static_cast<unsigned>(key));
    if (virtual_key == XPLM_VK_BACK)
        io.AddKeyEvent(ImGuiKey_Backspace, true);
    if (virtual_key == XPLM_VK_DELETE)
        io.AddKeyEvent(ImGuiKey_Delete, true);
    if (virtual_key == XPLM_VK_RETURN)
        io.AddKeyEvent(ImGuiKey_Enter, true);
    if (virtual_key == XPLM_VK_ESCAPE)
        close_window();
}

void draw_input_window(XPLMWindowID, void *) {}

void create_input_window()
{
    int left = 0, top = 0, right = 0, bottom = 0;
    XPLMGetScreenBoundsGlobal(&left, &top, &right, &bottom);

    XPLMCreateWindow_t parameters       = {};
    parameters.structSize               = sizeof(parameters);
    parameters.left                     = left;
    parameters.bottom                   = bottom;
    parameters.right                    = right;
    parameters.top                      = top;
    parameters.visible                  = 1;
    parameters.drawWindowFunc           = draw_input_window;
    parameters.handleMouseClickFunc     = forward_mouse;
    parameters.handleKeyFunc            = forward_key;
    parameters.handleCursorFunc         = forward_cursor;
    parameters.handleMouseWheelFunc     = forward_scroll;
    parameters.decorateAsFloatingWindow = xplm_WindowDecorationNone;
    parameters.layer                    = xplm_WindowLayerFloatingWindows;

    input_window = XPLMCreateWindowEx(&parameters);
}

void open_window()
{
    window_open = true;
    port_stored = false;
    port_field  = current_settings().port;

    if (input_window == nullptr)
        create_input_window();

    XPLMSetWindowIsVisible(input_window, 1);
    XPLMBringWindowToFront(input_window);
}

void close_window()
{
    window_open = false;
    if (input_window != nullptr)
        XPLMSetWindowIsVisible(input_window, 0);
}

void toggle_window(void *, void *)
{
    if (window_open)
        close_window();
    else
        open_window();
}

// ── Window contents ──────────────────────────────────────────────────────────

std::string tablet_url()
{
    const std::string address = local_network_address();
    if (address.empty())
        return {};

    return "http://" + address + ":" + std::to_string(current_settings().port) + "/";
}

void draw_address()
{
    ImGui::TextUnformatted("Address for your tablet");

    const std::string url = tablet_url();
    if (url.empty())
    {
        ImGui::TextUnformatted("This machine is not on a network.");
        return;
    }

    ImGui::SetWindowFontScale(URL_FONT_SCALE);
    ImGui::TextUnformatted(url.c_str());
    ImGui::SetWindowFontScale(1.0f);

    // Without a real clipboard the button would report success and paste
    // nothing, so it is simply absent. The URL stands above in 1.6x type,
    // which is what it is there for.
    if (CLIPBOARD_IS_REAL)
    {
        if (ImGui::Button("Copy"))
            ImGui::SetClipboardText(url.c_str());

        ImGui::SameLine();
    }
    ImGui::TextDisabled("on this machine: http://localhost:%d/", current_settings().port);
}

void draw_server_state()
{
    if (!server_is_running())
    {
        ImGui::TextColored(ImVec4(0.85f, 0.4f, 0.3f, 1.0f), "Server not running - is port %d taken?",
                           current_settings().port);
        return;
    }

    const int viewers = active_stream_count();
    ImGui::Text("Server listening on %s:%d, %d viewer%s", current_settings().bind_address.c_str(),
                current_settings().port, viewers, viewers == 1 ? "" : "s");
}

void draw_devices()
{
    ImGui::TextUnformatted("Devices");

    for (const DeviceDescriptor &device : all_devices())
    {
        bool enabled = device.enabled;
        if (ImGui::Checkbox(std::string(device.display_name).c_str(), &enabled))
        {
            set_device_enabled(device.slug, enabled);
            if (devices_changed != nullptr)
                devices_changed();
        }

        ImGui::SameLine(220.0f);
        if (!device.enabled)
            ImGui::TextDisabled("off");
        else if (device_is_in_aircraft(device.device_id))
            ImGui::TextUnformatted("in this aircraft");
        else
            ImGui::TextDisabled("not in this aircraft");
    }
}

void draw_port_setting()
{
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputInt("Port", &port_field);
    port_field = std::clamp(port_field, 1, 65535);

    ImGui::SameLine();
    if (ImGui::Button("Save"))
    {
        current_settings().port = port_field;
        port_stored             = persist_settings();
    }

    if (port_stored)
        ImGui::TextDisabled("Saved - takes effect after an X-Plane restart.");
}

void draw_security_note()
{
    ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), "No password protects this stream.");
    ImGui::TextWrapped("Everyone on this network can watch your avionics and operate them. "
                       "Set bind_address to 127.0.0.1 in the settings file to keep it on this machine.");
}

void draw_contents()
{
    ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH, WINDOW_HEIGHT), ImGuiCond_FirstUseEver);

    bool              stays_open = true;
    const std::string title      = std::string(PRODUCT_NAME) + " " + XP_GAUGE_STREAMER_VERSION + "##xp_gauge_streamer";

    if (ImGui::Begin(title.c_str(), &stays_open, ImGuiWindowFlags_NoCollapse))
    {
        draw_address();
        ImGui::Separator();
        draw_server_state();
        ImGui::Separator();
        draw_devices();
        ImGui::Separator();
        draw_port_setting();
        ImGui::Separator();
        draw_security_note();
    }
    ImGui::End();

    if (!stays_open)
        close_window();
}

// ── Frame ────────────────────────────────────────────────────────────────────

void keep_input_window_on_screen()
{
    int screen_left = 0, screen_top = 0, screen_right = 0, screen_bottom = 0;
    XPLMGetScreenBoundsGlobal(&screen_left, &screen_top, &screen_right, &screen_bottom);

    int left = 0, top = 0, right = 0, bottom = 0;
    XPLMGetWindowGeometry(input_window, &left, &top, &right, &bottom);

    if (left != screen_left || top != screen_top || right != screen_right || bottom != screen_bottom)
        XPLMSetWindowGeometry(input_window, screen_left, screen_top, screen_right, screen_bottom);
}

int render(XPLMDrawingPhase, int, void *)
{
    if (!window_open || imgui_context == nullptr)
        return 1;

    int screen_left = 0, screen_top = 0, screen_right = 0, screen_bottom = 0;
    XPLMGetScreenBoundsGlobal(&screen_left, &screen_top, &screen_right, &screen_bottom);

    const int screen_width  = screen_right - screen_left;
    const int screen_height = screen_top - screen_bottom;
    if (screen_width <= 0 || screen_height <= 0)
        return 1;

    keep_input_window_on_screen();

    // "The OpenGL state will be unknown" — save everything this touches.
    GLint saved_viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, saved_viewport);
    glPushAttrib(GL_TRANSFORM_BIT | GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_SCISSOR_BIT |
                 GL_TEXTURE_BIT);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // The framebuffer can be larger than the logical screen on a retina display.
    const int framebuffer_width  = saved_viewport[2];
    const int framebuffer_height = saved_viewport[3];

    glViewport(0, 0, framebuffer_width, framebuffer_height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, screen_width, screen_height, 0, -1, 1); // top-left origin, as ImGui expects
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    const double now           = XPLMGetElapsedTime();
    ImGuiIO     &io            = ImGui::GetIO();
    io.DeltaTime               = static_cast<float>(std::max(now - last_frame_time, 0.001));
    last_frame_time            = now;
    io.DisplaySize             = ImVec2(static_cast<float>(screen_width), static_cast<float>(screen_height));
    io.DisplayFramebufferScale = ImVec2(static_cast<float>(framebuffer_width) / static_cast<float>(screen_width),
                                        static_cast<float>(framebuffer_height) / static_cast<float>(screen_height));

    ImGui_ImplOpenGL2_NewFrame();
    ImGui::NewFrame();
    draw_contents();
    ImGui::Render();
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
    glViewport(saved_viewport[0], saved_viewport[1], saved_viewport[2], saved_viewport[3]);

    return 1;
}

} // namespace

void start_ui(DevicesChanged on_devices_changed)
{
    devices_changed = on_devices_changed;

    IMGUI_CHECKVERSION();
    imgui_context = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_context);

    ImGuiIO &io    = ImGui::GetIO();
    io.IniFilename = nullptr; // no imgui.ini next to the X-Plane binary
    io.LogFilename = nullptr;
    io.ConfigFlags = static_cast<ImGuiConfigFlags>(static_cast<unsigned>(io.ConfigFlags) |
                                                   static_cast<unsigned>(ImGuiConfigFlags_NoMouseCursorChange));

    ImGui::StyleColorsDark();
    ImGui_ImplOpenGL2_Init();
    last_frame_time = XPLMGetElapsedTime();

    const int menu_index = XPLMAppendMenuItem(XPLMFindPluginsMenu(), PRODUCT_NAME, nullptr, 0);
    plugin_menu          = XPLMCreateMenu(PRODUCT_NAME, XPLMFindPluginsMenu(), menu_index, toggle_window, nullptr);
    XPLMAppendMenuItem(plugin_menu, "Stream settings", nullptr, 0);

    XPLMRegisterDrawCallback(render, xplm_Phase_Window, 0, nullptr);
}

void stop_ui()
{
    XPLMUnregisterDrawCallback(render, xplm_Phase_Window, 0, nullptr);

    if (plugin_menu != nullptr)
    {
        XPLMDestroyMenu(plugin_menu);
        plugin_menu = nullptr;
    }

    if (input_window != nullptr)
    {
        XPLMDestroyWindow(input_window);
        input_window = nullptr;
    }

    if (imgui_context != nullptr)
    {
        ImGui_ImplOpenGL2_Shutdown();
        ImGui::DestroyContext(imgui_context);
        imgui_context = nullptr;
    }

    window_open = false;
}

} // namespace xp_gauge_streamer
