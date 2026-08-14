#include "frame_buffer.hpp"

#include <algorithm>

namespace xp_gauge_streamer
{

void FrameBuffer::publish(const unsigned char *rgb, int width, int height)
{
    const size_t byte_count = static_cast<size_t>(width) * static_cast<size_t>(height) * 3;

    {
        const std::lock_guard<std::mutex> lock(mutex);
        if (stopped)
            return;

        pending.pixels.resize(byte_count);
        std::copy_n(rgb, byte_count, pending.pixels.begin());
        pending.width    = width;
        pending.height   = height;
        pending.sequence = next_sequence++;
        has_pending      = true;
    }

    frame_arrived.notify_one();
}

bool FrameBuffer::take(Frame &into)
{
    std::unique_lock<std::mutex> lock(mutex);
    frame_arrived.wait(lock, [this] { return has_pending || stopped; });

    if (!has_pending)
        return false;

    // The consumer's buffer goes back to the producer instead of being freed.
    std::swap(pending, into);
    has_pending = false;
    return true;
}

void FrameBuffer::stop()
{
    {
        const std::lock_guard<std::mutex> lock(mutex);
        stopped = true;
    }

    frame_arrived.notify_all();
}

} // namespace xp_gauge_streamer
