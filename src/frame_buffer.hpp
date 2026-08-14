/*
 * xp_gauge_streamer - GNS430/530 display streamer for X-Plane 12
 * Copyright (c) 2026 thWelly
 *
 * Licensed under the MIT License. See the LICENSE file in the
 * project root for full license information.
 */

#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <vector>

namespace xp_gauge_streamer
{

struct Frame
{
    std::vector<unsigned char> pixels;
    int                        width    = 0;
    int                        height   = 0;
    std::uint64_t              sequence = 0;
};

// Hands the newest frame from the draw callback to an encoder thread.
//
// There is no queue: the producer overwrites whatever is still waiting. A GNS
// frame that nobody picked up is worthless by the time the next one arrives,
// and the draw callback must never wait on a consumer.
class FrameBuffer
{
  public:
    // Producer side. Never blocks, never allocates once the size settles.
    void publish(const unsigned char *rgb, int width, int height);

    // Consumer side. Blocks until a frame is waiting, then swaps it into
    // `into` — the caller's old pixel buffer becomes the producer's next one,
    // so neither side allocates in steady state.
    // Returns false once stop() was called and nothing is left to take.
    bool take(Frame &into);

    // Wakes every waiting consumer for good.
    void stop();

  private:
    std::mutex              mutex;
    std::condition_variable frame_arrived;
    Frame                   pending;
    bool                    has_pending   = false;
    bool                    stopped       = false;
    std::uint64_t           next_sequence = 1;
};

} // namespace xp_gauge_streamer
