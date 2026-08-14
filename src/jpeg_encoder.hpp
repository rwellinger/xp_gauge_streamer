/*
 * xp_gauge_streamer - GNS430/530 display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#pragma once

#include <cstddef>

namespace xp_gauge_streamer
{

// A JPEG owned by the encoder that produced it. Valid until the next encode()
// on that encoder, or until the encoder dies.
struct EncodedJpeg
{
    const unsigned char *bytes = nullptr;
    size_t               size  = 0;

    bool empty() const { return bytes == nullptr || size == 0; }
};

// RAII wrapper around a turbojpeg compressor. Holding the compressor across
// frames avoids re-initialising Huffman tables for every single frame.
class JpegEncoder
{
  public:
    // The GNS screen is hard-edged text and symbols on black — 4:2:0 chroma
    // subsampling would smear the coloured glyph edges, so the caller picks.
    JpegEncoder(int quality, bool subsample_chroma);
    ~JpegEncoder();

    JpegEncoder(const JpegEncoder &)            = delete;
    JpegEncoder &operator=(const JpegEncoder &) = delete;

    // Encodes bottom-up RGB, the way OpenGL hands it over. Returns an empty
    // result on failure.
    EncodedJpeg encode(const unsigned char *rgb, int width, int height);

  private:
    void          *compressor = nullptr; // tjhandle; kept opaque to spare callers the turbojpeg header
    unsigned char *jpeg_bytes = nullptr;
    size_t         jpeg_size  = 0;
};

} // namespace xp_gauge_streamer
