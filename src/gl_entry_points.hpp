/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

/*
 * The five pixel-buffer functions the capture needs. They are GL 1.5, which
 * Windows' opengl32 does not export — there they have to be resolved at
 * runtime. Every platform goes through the same pointers so that the calling
 * code never learns which one it is on.
 */

#pragma once

#include "opengl.hpp"

namespace xp_gauge_streamer
{

#if defined(_WIN32)
#define XP_GL_CALL APIENTRY
#else
#define XP_GL_CALL
#endif

extern void(XP_GL_CALL *gl_gen_buffers)(GLsizei count, GLuint *names);
extern void(XP_GL_CALL *gl_bind_buffer)(GLenum target, GLuint name);
extern void(XP_GL_CALL *gl_buffer_data)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
extern void *(XP_GL_CALL *gl_map_buffer)(GLenum target, GLenum access);
extern GLboolean(XP_GL_CALL *gl_unmap_buffer)(GLenum target);

// True once all five pointers are usable. Must be called with a current GL
// context — on Windows the loader returns null without one — which in this
// plugin means from inside a draw callback.
bool load_gl_entry_points();

} // namespace xp_gauge_streamer
