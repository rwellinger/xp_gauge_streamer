#include "frame_pipeline.hpp"

#include "frame_buffer.hpp"
#include "jpeg_encoder.hpp"
#include "plugin_log.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace xp_gauge_streamer
{

namespace
{

constexpr int  JPEG_QUALITY     = 80;
constexpr bool SUBSAMPLE_CHROMA = true;

struct DeviceStream
{
    DeviceId    device_id;
    std::string display_name;
    FrameBuffer frames;

    std::mutex                 jpeg_mutex;
    std::vector<unsigned char> jpeg;
    std::uint64_t              jpeg_sequence = 0;

    std::thread encoder;
};

// Held by pointer: each stream owns a mutex and a thread, neither of which
// survives the moves a growing vector of values would perform.
std::vector<std::unique_ptr<DeviceStream>> streams;

// The HTTP server reads frames from civetweb's threads while the main thread
// starts and stops the pipeline as viewers come and go. Guards the vector, not
// a stream's contents — those have their own mutex.
std::mutex streams_mutex;

// Callers must hold streams_mutex; the returned stream stays valid only for as
// long as they do.
DeviceStream *find_stream(DeviceId device_id)
{
    for (const std::unique_ptr<DeviceStream> &stream : streams)
    {
        if (stream->device_id == device_id)
            return stream.get();
    }
    return nullptr;
}

void encode_frames(DeviceStream &stream)
{
    JpegEncoder encoder(JPEG_QUALITY, SUBSAMPLE_CHROMA);
    Frame       frame;

    while (stream.frames.take(frame))
    {
        const EncodedJpeg encoded = encoder.encode(frame.pixels.data(), frame.width, frame.height);
        if (encoded.empty())
            continue;

        const std::lock_guard<std::mutex> lock(stream.jpeg_mutex);
        stream.jpeg.assign(encoded.bytes, encoded.bytes + encoded.size);
        stream.jpeg_sequence = frame.sequence;
    }
}

} // namespace

void start_pipeline()
{
    const std::lock_guard<std::mutex> lock(streams_mutex);

    for (const DeviceDescriptor &descriptor : all_devices())
    {
        if (!descriptor.enabled || find_stream(descriptor.device_id) != nullptr)
            continue;

        auto stream          = std::make_unique<DeviceStream>();
        stream->device_id    = descriptor.device_id;
        stream->display_name = std::string(descriptor.display_name);

        DeviceStream &started = *stream;
        streams.push_back(std::move(stream));
        started.encoder = std::thread([&started] { encode_frames(started); });

        log_format("%s: encoder thread started", started.display_name.c_str());
    }
}

void stop_pipeline()
{
    const std::lock_guard<std::mutex> lock(streams_mutex);

    for (const std::unique_ptr<DeviceStream> &stream : streams)
    {
        stream->frames.stop();
        if (stream->encoder.joinable())
            stream->encoder.join();
    }

    streams.clear();
}

void publish_frame(DeviceId device_id, const unsigned char *rgb, int width, int height)
{
    const std::lock_guard<std::mutex> lock(streams_mutex);

    DeviceStream *stream = find_stream(device_id);
    if (stream != nullptr)
        stream->frames.publish(rgb, width, height);
}

bool has_frames(DeviceId device_id)
{
    const std::lock_guard<std::mutex> streams_lock(streams_mutex);

    DeviceStream *stream = find_stream(device_id);
    if (stream == nullptr)
        return false;

    const std::lock_guard<std::mutex> jpeg_lock(stream->jpeg_mutex);
    return !stream->jpeg.empty();
}

bool latest_jpeg(DeviceId device_id, std::vector<unsigned char> &into, std::uint64_t &sequence)
{
    const std::lock_guard<std::mutex> streams_lock(streams_mutex);

    DeviceStream *stream = find_stream(device_id);
    if (stream == nullptr)
        return false;

    const std::lock_guard<std::mutex> jpeg_lock(stream->jpeg_mutex);
    if (stream->jpeg.empty())
        return false;

    into     = stream->jpeg;
    sequence = stream->jpeg_sequence;
    return true;
}

} // namespace xp_gauge_streamer
