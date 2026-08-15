/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#include "gl_entry_points.hpp"

#include "plugin_log.hpp"

#if !defined(__APPLE__) && !defined(_WIN32)
#include <dlfcn.h>
#endif

namespace xp_gauge_streamer
{

void(XP_GL_CALL *gl_gen_buffers)(GLsizei, GLuint *)                        = nullptr;
void(XP_GL_CALL *gl_bind_buffer)(GLenum, GLuint)                           = nullptr;
void(XP_GL_CALL *gl_buffer_data)(GLenum, GLsizeiptr, const void *, GLenum) = nullptr;
void *(XP_GL_CALL *gl_map_buffer)(GLenum, GLenum)                          = nullptr;
GLboolean(XP_GL_CALL *gl_unmap_buffer)(GLenum)                             = nullptr;

#if defined(__APPLE__)

// The macOS header declares all five, so the addresses are known at link time.
// The pointers exist only so that the calling code stays platform-free.
bool load_gl_entry_points()
{
    gl_gen_buffers  = &glGenBuffers;
    gl_bind_buffer  = &glBindBuffer;
    gl_buffer_data  = &glBufferData;
    gl_map_buffer   = &glMapBuffer;
    gl_unmap_buffer = &glUnmapBuffer;
    return true;
}

#else

namespace
{

#if defined(_WIN32)

// opengl32.dll exports only GL 1.1; anything newer comes from the driver via
// wglGetProcAddress, which needs a current context.
void *resolve(const char *name)
{
    if (void *address = reinterpret_cast<void *>(wglGetProcAddress(name)))
        return address;

    static HMODULE opengl32 = LoadLibraryA("opengl32.dll");
    if (opengl32 == nullptr)
        return nullptr;

    return reinterpret_cast<void *>(GetProcAddress(opengl32, name));
}

#else

// X-Plane has libGL loaded already, so a process-wide lookup finds the symbols
// without this plugin linking or opening the library itself.
void *resolve(const char *name)
{
    if (void *address = dlsym(RTLD_DEFAULT, name))
        return address;

    using GetProcAddress = void *(*)(const unsigned char *);
    auto from_glx        = reinterpret_cast<GetProcAddress>(dlsym(RTLD_DEFAULT, "glXGetProcAddressARB"));
    if (from_glx == nullptr)
        return nullptr;

    return from_glx(reinterpret_cast<const unsigned char *>(name));
}

#endif

} // namespace

bool load_gl_entry_points()
{
    gl_gen_buffers  = reinterpret_cast<decltype(gl_gen_buffers)>(resolve("glGenBuffers"));
    gl_bind_buffer  = reinterpret_cast<decltype(gl_bind_buffer)>(resolve("glBindBuffer"));
    gl_buffer_data  = reinterpret_cast<decltype(gl_buffer_data)>(resolve("glBufferData"));
    gl_map_buffer   = reinterpret_cast<decltype(gl_map_buffer)>(resolve("glMapBuffer"));
    gl_unmap_buffer = reinterpret_cast<decltype(gl_unmap_buffer)>(resolve("glUnmapBuffer"));

    const bool complete = gl_gen_buffers != nullptr && gl_bind_buffer != nullptr && gl_buffer_data != nullptr &&
                          gl_map_buffer != nullptr && gl_unmap_buffer != nullptr;

    if (!complete)
        log_format("OpenGL pixel buffer functions unavailable — capture cannot run");

    return complete;
}

#endif

} // namespace xp_gauge_streamer
