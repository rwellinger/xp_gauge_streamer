/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

/*
 * The one place in the spike that knows about platforms: the GL header, the
 * GL 1.5 constants a legacy header lacks, and the six pixel-buffer entry points.
 *
 * macOS declares all six in its legacy GL 2.1 header, so the plain names work.
 * Everywhere else they are resolved at runtime: Windows links opengl32.lib,
 * which stops at GL 1.1, and the Linux header only declares them when
 * GL_GLEXT_PROTOTYPES is set — asking the loader works on both without relying
 * on which glext the distribution ships. This is the shape issue #27 will give
 * the production code; the spike proves the loader before that work starts.
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

#if !defined(__APPLE__)
#if defined(_WIN32)
// The Windows GL 1.1 header predates buffer objects and has no pointer-sized
// integer types for them.
#include <cstddef>
using GLsizeiptr = std::ptrdiff_t;
using GLintptr   = std::ptrdiff_t;

// Only Windows puts GL entry points on __stdcall.
#define SPIKE_GLAPIENTRY APIENTRY
#else
#define SPIKE_GLAPIENTRY
#endif

using PFN_glGenBuffers    = void(SPIKE_GLAPIENTRY *)(GLsizei, GLuint *);
using PFN_glDeleteBuffers = void(SPIKE_GLAPIENTRY *)(GLsizei, const GLuint *);
using PFN_glBindBuffer    = void(SPIKE_GLAPIENTRY *)(GLenum, GLuint);
using PFN_glBufferData    = void(SPIKE_GLAPIENTRY *)(GLenum, GLsizeiptr, const void *, GLenum);
using PFN_glMapBuffer     = void *(SPIKE_GLAPIENTRY *)(GLenum, GLenum);
using PFN_glUnmapBuffer   = GLboolean(SPIKE_GLAPIENTRY *)(GLenum);

extern PFN_glGenBuffers    spike_glGenBuffers;
extern PFN_glDeleteBuffers spike_glDeleteBuffers;
extern PFN_glBindBuffer    spike_glBindBuffer;
extern PFN_glBufferData    spike_glBufferData;
extern PFN_glMapBuffer     spike_glMapBuffer;
extern PFN_glUnmapBuffer   spike_glUnmapBuffer;

#define glGenBuffers spike_glGenBuffers
#define glDeleteBuffers spike_glDeleteBuffers
#define glBindBuffer spike_glBindBuffer
#define glBufferData spike_glBufferData
#define glMapBuffer spike_glMapBuffer
#define glUnmapBuffer spike_glUnmapBuffer
#endif

// True when all six entry points are usable. Call from inside a draw callback:
// wglGetProcAddress needs a current context and returns null without one.
bool load_gl_entry_points();

// Where the entry points came from, for the log line that proves it.
const char *gl_entry_point_source();
