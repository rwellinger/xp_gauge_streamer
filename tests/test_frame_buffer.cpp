/*
 * xp_gauge_streamer - avionics display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#include "frame_buffer.hpp"

#include <catch_amalgamated.hpp>

#include <atomic>
#include <thread>
#include <vector>

using namespace xp_gauge_streamer;

namespace
{

std::vector<unsigned char> solid_image(int width, int height, unsigned char value)
{
    return std::vector<unsigned char>(static_cast<size_t>(width) * static_cast<size_t>(height) * 3, value);
}

} // namespace

TEST_CASE("a published frame arrives with its dimensions", "[frame_buffer]")
{
    FrameBuffer                buffer;
    const std::vector<unsigned char> image = solid_image(4, 2, 0x7F);

    buffer.publish(image.data(), 4, 2);

    Frame taken;
    REQUIRE(buffer.take(taken));
    CHECK(taken.width == 4);
    CHECK(taken.height == 2);
    CHECK(taken.pixels == image);
    CHECK(taken.sequence == 1);
}

TEST_CASE("sequence numbers count every published frame", "[frame_buffer]")
{
    FrameBuffer                buffer;
    const std::vector<unsigned char> image = solid_image(2, 2, 0x01);

    buffer.publish(image.data(), 2, 2);
    Frame first;
    REQUIRE(buffer.take(first));

    buffer.publish(image.data(), 2, 2);
    Frame second;
    REQUIRE(buffer.take(second));

    CHECK(first.sequence == 1);
    CHECK(second.sequence == 2);
}

TEST_CASE("a fast producer overwrites the waiting frame", "[frame_buffer]")
{
    FrameBuffer buffer;

    buffer.publish(solid_image(2, 2, 0x11).data(), 2, 2);
    buffer.publish(solid_image(2, 2, 0x22).data(), 2, 2);
    buffer.publish(solid_image(2, 2, 0x33).data(), 2, 2);

    Frame taken;
    REQUIRE(buffer.take(taken));

    // Only the newest survives, and the gap is visible in the sequence number.
    CHECK(taken.pixels == solid_image(2, 2, 0x33));
    CHECK(taken.sequence == 3);
}

TEST_CASE("a frame published after a size change carries the new size", "[frame_buffer]")
{
    FrameBuffer buffer;

    buffer.publish(solid_image(8, 8, 0x44).data(), 8, 8);
    buffer.publish(solid_image(2, 2, 0x55).data(), 2, 2);

    Frame taken;
    REQUIRE(buffer.take(taken));
    CHECK(taken.width == 2);
    CHECK(taken.height == 2);
    CHECK(taken.pixels.size() == 2 * 2 * 3);
}

TEST_CASE("the consumer blocks until a frame is published", "[frame_buffer]")
{
    FrameBuffer      buffer;
    std::atomic<int> taken_count{0};

    std::thread consumer(
        [&buffer, &taken_count]
        {
            Frame frame;
            while (buffer.take(frame))
                ++taken_count;
        });

    for (int frame = 0; frame < 20; ++frame)
    {
        buffer.publish(solid_image(2, 2, static_cast<unsigned char>(frame)).data(), 2, 2);
        std::this_thread::yield();
    }

    buffer.stop();
    consumer.join();

    // The producer never waits, so some frames are legitimately overwritten —
    // what matters is that the consumer saw frames and the thread ended.
    CHECK(taken_count > 0);
    CHECK(taken_count <= 20);
}

TEST_CASE("stop releases a consumer that is waiting on an empty buffer", "[frame_buffer]")
{
    FrameBuffer       buffer;
    std::atomic<bool> returned{false};
    std::atomic<bool> got_frame{true};

    // Catch2 assertions are not thread-safe, so the result is carried back and
    // checked here rather than inside the consumer.
    std::thread consumer(
        [&buffer, &returned, &got_frame]
        {
            Frame frame;
            got_frame = buffer.take(frame);
            returned  = true;
        });

    buffer.stop();
    consumer.join();

    CHECK(returned);
    CHECK_FALSE(got_frame);
}

TEST_CASE("a frame published before stop is still delivered", "[frame_buffer]")
{
    FrameBuffer buffer;
    buffer.publish(solid_image(2, 2, 0x66).data(), 2, 2);
    buffer.stop();

    Frame taken;
    CHECK(buffer.take(taken));
    CHECK_FALSE(buffer.take(taken));
}

TEST_CASE("publishing after stop is ignored", "[frame_buffer]")
{
    FrameBuffer buffer;
    buffer.stop();

    buffer.publish(solid_image(2, 2, 0x77).data(), 2, 2);

    Frame taken;
    CHECK_FALSE(buffer.take(taken));
}
