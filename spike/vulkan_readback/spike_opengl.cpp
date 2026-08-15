/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#include "spike_opengl.hpp"

#if defined(__APPLE__)

bool        load_gl_entry_points() { return true; }
const char *gl_entry_point_source() { return "linked directly"; }

#else

// The macro aliases from the header would rewrite the definitions below into
// assignments to themselves.
#undef glGenBuffers
#undef glDeleteBuffers
#undef glBindBuffer
#undef glBufferData
#undef glMapBuffer
#undef glUnmapBuffer

PFN_glGenBuffers    spike_glGenBuffers    = nullptr;
PFN_glDeleteBuffers spike_glDeleteBuffers = nullptr;
PFN_glBindBuffer    spike_glBindBuffer    = nullptr;
PFN_glBufferData    spike_glBufferData    = nullptr;
PFN_glMapBuffer     spike_glMapBuffer     = nullptr;
PFN_glUnmapBuffer   spike_glUnmapBuffer   = nullptr;

namespace
{

const char *entry_point_source = "not loaded";

#if defined(_WIN32)

// wglGetProcAddress answers only for functions beyond GL 1.1; the rest live as
// plain exports of opengl32.dll. Trying both is what every loader library does.
void *resolve(const char *name)
{
    if (void *address = reinterpret_cast<void *>(wglGetProcAddress(name)))
        return address;

    static HMODULE opengl32 = LoadLibraryA("opengl32.dll");
    if (opengl32 == nullptr)
        return nullptr;

    return reinterpret_cast<void *>(GetProcAddress(opengl32, name));
}

const char *source_label() { return "wglGetProcAddress"; }

#else

#include <dlfcn.h>

// X-Plane has libGL loaded already, so the process-wide lookup finds the
// symbols without this plugin linking or dlopening the library itself.
void *resolve(const char *name)
{
    if (void *address = dlsym(RTLD_DEFAULT, name))
        return address;

    using PFN_glXGetProcAddressARB = void *(*) (const unsigned char *);
    auto get_proc_address = reinterpret_cast<PFN_glXGetProcAddressARB>(dlsym(RTLD_DEFAULT, "glXGetProcAddressARB"));
    if (get_proc_address == nullptr)
        return nullptr;

    return get_proc_address(reinterpret_cast<const unsigned char *>(name));
}

const char *source_label() { return "dlsym/glXGetProcAddressARB"; }

#endif

} // namespace

bool load_gl_entry_points()
{
    spike_glGenBuffers    = reinterpret_cast<PFN_glGenBuffers>(resolve("glGenBuffers"));
    spike_glDeleteBuffers = reinterpret_cast<PFN_glDeleteBuffers>(resolve("glDeleteBuffers"));
    spike_glBindBuffer    = reinterpret_cast<PFN_glBindBuffer>(resolve("glBindBuffer"));
    spike_glBufferData    = reinterpret_cast<PFN_glBufferData>(resolve("glBufferData"));
    spike_glMapBuffer     = reinterpret_cast<PFN_glMapBuffer>(resolve("glMapBuffer"));
    spike_glUnmapBuffer   = reinterpret_cast<PFN_glUnmapBuffer>(resolve("glUnmapBuffer"));

    const bool complete = spike_glGenBuffers != nullptr && spike_glDeleteBuffers != nullptr &&
                          spike_glBindBuffer != nullptr && spike_glBufferData != nullptr &&
                          spike_glMapBuffer != nullptr && spike_glUnmapBuffer != nullptr;

    entry_point_source = complete ? source_label() : "not loaded";
    return complete;
}

const char *gl_entry_point_source() { return entry_point_source; }

#endif
