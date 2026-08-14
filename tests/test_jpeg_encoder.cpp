/*
 * xp_gauge_streamer - GNS430/530 display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#include "jpeg_encoder.hpp"

#include <catch_amalgamated.hpp>

#include <vector>

using namespace xp_gauge_streamer;

namespace
{

constexpr int QUALITY = 80;

// Vertical colour bars: hard edges are what a GNS screen actually shows, and
// they compress differently from a flat fill.
std::vector<unsigned char> colour_bars(int width, int height)
{
    std::vector<unsigned char> image(static_cast<size_t>(width) * static_cast<size_t>(height) * 3);

    for (int row = 0; row < height; ++row)
    {
        for (int column = 0; column < width; ++column)
        {
            const size_t offset = (static_cast<size_t>(row) * static_cast<size_t>(width) + column) * 3;
            image[offset]       = static_cast<unsigned char>((column % 8) * 32);
            image[offset + 1]   = static_cast<unsigned char>((row % 8) * 32);
            image[offset + 2]   = 0x40;
        }
    }

    return image;
}

bool starts_with_jpeg_signature(const EncodedJpeg &encoded)
{
    return encoded.size >= 3 && encoded.bytes[0] == 0xFF && encoded.bytes[1] == 0xD8 && encoded.bytes[2] == 0xFF;
}

} // namespace

TEST_CASE("encoding produces a JPEG", "[jpeg_encoder]")
{
    JpegEncoder                      encoder(QUALITY, true);
    const std::vector<unsigned char> image = colour_bars(64, 32);

    const EncodedJpeg encoded = encoder.encode(image.data(), 64, 32);

    REQUIRE_FALSE(encoded.empty());
    CHECK(starts_with_jpeg_signature(encoded));
    // Compressed, but not suspiciously tiny.
    CHECK(encoded.size < image.size());
    CHECK(encoded.size > 100);
}

TEST_CASE("the encoder survives repeated use", "[jpeg_encoder]")
{
    JpegEncoder                      encoder(QUALITY, true);
    const std::vector<unsigned char> image = colour_bars(32, 32);

    size_t first_size = 0;
    for (int round = 0; round < 20; ++round)
    {
        const EncodedJpeg encoded = encoder.encode(image.data(), 32, 32);
        REQUIRE_FALSE(encoded.empty());
        REQUIRE(starts_with_jpeg_signature(encoded));

        if (round == 0)
            first_size = encoded.size;

        // Identical input, identical output — no state bleeding between frames.
        CHECK(encoded.size == first_size);
    }
}

TEST_CASE("changing dimensions between frames works", "[jpeg_encoder]")
{
    JpegEncoder encoder(QUALITY, true);

    const std::vector<unsigned char> large = colour_bars(128, 64);
    const std::vector<unsigned char> small = colour_bars(16, 16);

    REQUIRE_FALSE(encoder.encode(large.data(), 128, 64).empty());
    const EncodedJpeg after_shrink = encoder.encode(small.data(), 16, 16);

    REQUIRE_FALSE(after_shrink.empty());
    CHECK(starts_with_jpeg_signature(after_shrink));
}

TEST_CASE("subsampling yields a smaller file than full chroma", "[jpeg_encoder]")
{
    const std::vector<unsigned char> image = colour_bars(64, 64);

    JpegEncoder       subsampled(QUALITY, true);
    JpegEncoder       full_chroma(QUALITY, false);
    const EncodedJpeg small = subsampled.encode(image.data(), 64, 64);
    const EncodedJpeg large = full_chroma.encode(image.data(), 64, 64);

    REQUIRE_FALSE(small.empty());
    REQUIRE_FALSE(large.empty());
    CHECK(small.size < large.size);
}

TEST_CASE("invalid input is rejected instead of crashing", "[jpeg_encoder]")
{
    JpegEncoder                      encoder(QUALITY, true);
    const std::vector<unsigned char> image = colour_bars(8, 8);

    CHECK(encoder.encode(nullptr, 8, 8).empty());
    CHECK(encoder.encode(image.data(), 0, 8).empty());
    CHECK(encoder.encode(image.data(), 8, -1).empty());
}
