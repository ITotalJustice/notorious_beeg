// Copyright 2022 TotalJustice.
// SPDX-License-Identifier: GPL-3.0-only

// this code is mostly from an old audio player i made
// for the switch, was never released however

// the "voice" can be created with any sample rate
// and the audio hw handles the resampling to the device output
// this means it can take the 4 sample rates [32768, 65536, 131072, 262144]
// supported by gba, however sampling at anything higher than 65k is taxing.

// theres no smart handling if too many samples are created.
// if this does happen, samples are dropped.

// theres very basic time stretching which stretches the
// last sample if there not enough samples, this actually sounds
// very good!

#include "../../../system.hpp"
#include <switch.h>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <mutex>
#include <array>
#include <vector>
#include <algorithm>
#include <ranges>
#include <new>

namespace {

// custom allocator for std::vector that respects pool alignment.
template <typename Type>
class PoolAllocator {
public:
    using value_type = Type; // used by std::vector

public:
    constexpr PoolAllocator() = default;
    constexpr ~PoolAllocator() = default;

    [[nodiscard]]
    constexpr auto allocate(const std::size_t n) -> Type* {
        return new(align) Type[n];
    }

    constexpr auto deallocate(Type* p, const std::size_t n) noexcept -> void {
        delete[] (align, p);
    }

private:
    static constexpr std::align_val_t align{0x1000};
};

constexpr AudioRendererConfig cfg = {
    .output_rate     = AudioRendererOutputRate_48kHz,
    .num_voices      = 2,
    .num_effects     = 0,
    .num_sinks       = 1,
    .num_mix_objs    = 1,
    .num_mix_buffers = 2,
};

AudioDriver driver;

constexpr int voice_id = 0;
constexpr int channels = 2;
constexpr int samples = 4096 * 2;
constexpr int frequency = 65536;
constexpr uint8_t sink_channels[channels]{ 0, 1 };

int sink_id;
int mem_pool_id;

// mempool that is aligned for audio
std::vector<std::uint8_t, PoolAllocator<std::uint8_t>> mem_pool;
std::size_t spec_size;

// this is what we write the samples into
std::vector<std::int16_t> temp_buf;
std::size_t temp_buffer_index;

// this is what we copy the temp_buf into per audio frame
std::array<AudioDriverWaveBuf, 2> wave_buffers;
std::size_t wave_buffer_index;

std::jthread thread;
std::mutex mutex;

auto audio_callback(void* user, int16_t left, int16_t right) -> void
{
    std::scoped_lock lock{mutex};

    if (temp_buffer_index >= temp_buf.size())
    {
        return;
    }

    temp_buf[temp_buffer_index++] = left;
    temp_buf[temp_buffer_index++] = right;
}

auto audio_thread(std::stop_token token) -> void
{
    while (!token.stop_requested())
    {
        auto& buffer = wave_buffers[wave_buffer_index];
        if (buffer.state == AudioDriverWaveBufState_Free || buffer.state == AudioDriverWaveBufState_Done)
        {
            // apparently mem_pool data shouldn't be used directly (opus example).
            // so we use a temp buffer then copy in the buffer.
            auto data = mem_pool.data() + (wave_buffer_index * spec_size);
            {
                std::scoped_lock lock{mutex};
                auto data16 = reinterpret_cast<int16_t*>(data);
                std::ranges::copy(temp_buf, data16);

                // stretch last sample
                if (temp_buffer_index >= 2 && temp_buffer_index < temp_buf.size())
                {
                    for (size_t i = temp_buffer_index; i < temp_buf.size(); i += 2)
                    {
                        data16[i+0] = temp_buf[temp_buffer_index - 2]; // left
                        data16[i+1] = temp_buf[temp_buffer_index - 1]; // right
                    }
                }

                temp_buffer_index = 0;
            }
            armDCacheFlush(data, spec_size);

            if (!audrvVoiceAddWaveBuf(&driver, voice_id, &buffer))
            {
                printf("[ERROR] failed to add wave buffer to voice!\n");
            }

            // resume voice is it was idle or stopped playing.
            if (!audrvVoiceIsPlaying(&driver, voice_id))
            {
                audrvVoiceStart(&driver, voice_id);
            }

            // advance the buffer index
            wave_buffer_index = (wave_buffer_index + 1) % wave_buffers.size();
        }

        if (auto r = audrvUpdate(&driver); R_FAILED(r))
        {
            printf("[ERROR] failed to update audio driver in loop!\n");
        }

        audrenWaitFrame();
    }
}

} // namespace

namespace nx::audio {

auto init() -> bool
{
    if (auto r = audrenInitialize(&cfg); R_FAILED(r))
    {
        printf("failed to init audren\n");
        return false;
    }

    if (auto r = audrvCreate(&driver, &cfg, channels); R_FAILED(r))
    {
        printf("failed to create driver\n");
        return false;
    }

    sink_id = audrvDeviceSinkAdd(&driver, AUDREN_DEFAULT_DEVICE_NAME, channels, sink_channels);

    if (auto r = audrvUpdate(&driver); R_FAILED(r))
    {
        printf("failed to add sink to driver\n");
        return false;
    }

    if (auto r = audrenStartAudioRenderer(); R_FAILED(r))
    {
        printf("failed to start audio renderer\n");
        return false;
    }

    if (!audrvVoiceInit(&driver, voice_id, channels, PcmFormat_Int16, frequency))
    {
        printf("failed to init voice\n");
        return false;
    }

    audrvVoiceSetDestinationMix(&driver, voice_id, AUDREN_FINAL_MIX_ID);
    if (channels == 1)
    {
        audrvVoiceSetMixFactor(&driver, voice_id, 1.0F, 0, 0);
        audrvVoiceSetMixFactor(&driver, voice_id, 1.0F, 0, 1);
    }
    else
    {
        audrvVoiceSetMixFactor(&driver, voice_id, 1.0F, 0, 0);
        audrvVoiceSetMixFactor(&driver, voice_id, 0.0F, 0, 1);
        audrvVoiceSetMixFactor(&driver, voice_id, 0.0F, 1, 0);
        audrvVoiceSetMixFactor(&driver, voice_id, 1.0F, 1, 1);
    }

    spec_size = sizeof(std::int16_t) * channels * samples;
    const auto mem_pool_size = ((spec_size * wave_buffers.size()) + (AUDREN_MEMPOOL_ALIGNMENT - 1)) &~ (AUDREN_MEMPOOL_ALIGNMENT - 1);
    mem_pool.resize(mem_pool_size);

    for (std::size_t i = 0; i < wave_buffers.size(); ++i)
    {
        wave_buffers[i].data_adpcm = mem_pool.data();
        wave_buffers[i].size = mem_pool.size();
        wave_buffers[i].start_sample_offset = i * samples;
        wave_buffers[i].end_sample_offset = wave_buffers[i].start_sample_offset + samples;
    }

    armDCacheFlush(mem_pool.data(), mem_pool.size());

    mem_pool_id = audrvMemPoolAdd(&driver, mem_pool.data(), mem_pool.size());
    if (!audrvMemPoolAttach(&driver, mem_pool_id))
    {
        printf("[ERROR] failed to attach mem pool!\n");
        return false;
    }

    wave_buffer_index = 0;

    // set the buffer size
    temp_buf.resize(spec_size / 2); // this is s16
    std::ranges::fill(temp_buf, 0);

    // start audio thread
    thread = std::jthread(audio_thread);

    // set callback for emu
    sys::System::gameboy_advance.set_audio_callback(audio_callback);

    return true;
}

auto quit() -> void
{
    // this may take a while to join!
    // its probably possible to wait on an audio event
    // which if thats the case, then i can manually wake up
    // the thread whenever i want.
    thread.request_stop();
    thread.join();
    printf("[INFO] joined audio loop thread\n");

    if (auto r = audrenStopAudioRenderer(); R_FAILED(r))
    {
        printf("[ERROR] failed to stop audren!\n");
    }

    audrvVoiceDrop(&driver, voice_id);
    audrvClose(&driver);
    audrenExit();

    mem_pool.clear();
}

} // namespace nx::audio
