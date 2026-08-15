/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#include "jpeg_encoder.hpp"

#include <turbojpeg.h>

namespace xp_gauge_streamer
{

JpegEncoder::JpegEncoder(int quality, bool subsample_chroma) : compressor(tj3Init(TJINIT_COMPRESS))
{
    if (compressor == nullptr)
        return;

    auto *handle = static_cast<tjhandle>(compressor);
    tj3Set(handle, TJPARAM_QUALITY, quality);
    tj3Set(handle, TJPARAM_SUBSAMP, subsample_chroma ? TJSAMP_420 : TJSAMP_444);
    // OpenGL's origin is bottom-left, so let turbojpeg flip instead of us.
    tj3Set(handle, TJPARAM_BOTTOMUP, 1);
}

JpegEncoder::~JpegEncoder()
{
    tj3Free(jpeg_bytes);

    if (compressor != nullptr)
        tj3Destroy(static_cast<tjhandle>(compressor));
}

EncodedJpeg JpegEncoder::encode(const unsigned char *rgb, int width, int height)
{
    if (compressor == nullptr || rgb == nullptr || width <= 0 || height <= 0)
        return {};

    // tj3Compress8 allocates the output itself when handed a null pointer, so
    // the previous frame's buffer is released here rather than leaking.
    tj3Free(jpeg_bytes);
    jpeg_bytes = nullptr;
    jpeg_size  = 0;

    const int pitch = 0; // tightly packed rows
    if (tj3Compress8(static_cast<tjhandle>(compressor), rgb, width, pitch, height, TJPF_RGB, &jpeg_bytes, &jpeg_size) !=
        0)
    {
        tj3Free(jpeg_bytes);
        jpeg_bytes = nullptr;
        jpeg_size  = 0;
        return {};
    }

    return {jpeg_bytes, jpeg_size};
}

} // namespace xp_gauge_streamer
