/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#pragma once

namespace xp_gauge_streamer
{

// The "About the developer" window, shared with thWelly's other plugins. Its
// content lives in the xp_wellys_about repo; this side only supplies the
// plugin's name, version and license notice.

// Draws the window while `open` is true; its close button clears it. Call inside
// the ImGui frame of an X-Plane draw callback.
void draw_about_window(bool &open);

// Frees the photo texture. Call before the ImGui renderer shuts down.
void release_about();

} // namespace xp_gauge_streamer
