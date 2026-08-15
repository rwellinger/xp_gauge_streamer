/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

/*
 * The only place in this plugin that includes an OpenGL header. Each platform
 * spells it differently, and Windows additionally needs <windows.h> first for
 * the calling-convention macros the GL header relies on.
 *
 * It also carries the GL 1.5 names that the headers may predate: the pixel
 * buffer constants and, on Windows, the pointer-sized integer types. The
 * functions behind those constants live in gl_entry_points.hpp.
 */

#pragma once

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#elif defined(_WIN32)
#include <windows.h>
// clang-format off
#include <GL/gl.h>
// clang-format on
#else
#include <GL/gl.h>
#endif

#if defined(_WIN32)
// The Windows GL header stops at 1.1 and has no pointer-sized integer types.
#include <cstddef>
using GLsizeiptr = std::ptrdiff_t;
using GLintptr   = std::ptrdiff_t;
#endif

// The values are fixed by the specification and identical on every platform.
#ifndef GL_PIXEL_PACK_BUFFER
#define GL_PIXEL_PACK_BUFFER 0x88EB
#endif
#ifndef GL_PIXEL_PACK_BUFFER_BINDING
#define GL_PIXEL_PACK_BUFFER_BINDING 0x88ED
#endif
#ifndef GL_STREAM_READ
#define GL_STREAM_READ 0x88E1
#endif
#ifndef GL_READ_ONLY
#define GL_READ_ONLY 0x88B8
#endif
