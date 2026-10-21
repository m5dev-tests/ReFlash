#include "reflash/audio_engine.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace reflash {

static const int ADPCM_STEP_TABLE[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static const int ADPCM_INDEX_TABLE_2[2] = { -1, 2 };
static const int ADPCM_INDEX_TABLE_3[4] = { -1, -1, 2, 4 };
static const int ADPCM_INDEX_TABLE_4[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };
static const int ADPCM_INDEX_TABLE_5[16] = { -1, -1, -1, -1, -1, -1, -1, -1, 1, 2, 4, 6, 8, 10, 12, 14 };

int SoundStreamHeader::sample_rate_hz() const {
    switch (stream_sound_rate) {
        case 0: return 5512;
        case 1: return 11025;
        case 2: return 22050;
        case 3: return 44100;
        default: return 44100;
    }
}

SoundStreamHeader AudioEngine::parse_sound_stream_head(SWFStream& stream, uint16_t tag_code, uint32_t tag_length) {
    SoundStreamHeader header;
    if (tag_length < 4) {
        return header;
    }

    uint8_t b0 = stream.read_u8();
    uint8_t b1 = stream.read_u8();
    uint16_t sample_count = stream.read_u16_le();

    // Byte 0: Reserved (4 bits), PlaybackSoundRate (2 bits), PlaybackSoundSize (1 bit), PlaybackSoundType (1 bit)
    header.playback_sound_rate = (b0 >> 2) & 0x03;
    header.playback_sound_size = (b0 >> 1) & 0x01;
    header.playback_sound_type = b0 & 0x01;

    // Byte 1: StreamSoundCompression (4 bits), StreamSoundRate (2 bits), StreamSoundSize (1 bit), StreamSoundType (1 bit)
    header.stream_sound_compression = static_cast<SoundCompression>((b1 >> 4) & 0x0F);
    header.stream_sound_rate = (b1 >> 2) & 0x03;
    header.stream_sound_size = (b1 >> 1) & 0x01;
    header.stream_sound_type = b1 & 0x01;
    header.stream_sound_sample_count = sample_count;

    if (tag_code == 45 && header.stream_sound_compression == SoundCompression::MP3 && tag_length >= 6) {
        header.latency_seek = stream.read_u16_le();
    }

    header.is_valid = true;
    return header;
}

void AudioEngine::decode_raw_pcm(const uint8_t* data, size_t data_len, uint8_t sample_size, std::vector<int16_t>& out_samples) {
    if (sample_size == 0) { // 8-bit unsigned PCM
        out_samples.reserve(out_samples.size() + data_len);
        for (size_t i = 0; i < data_len; ++i) {
            // Convert uint8 (0..255) to int16 (-32768..32767)
            int16_t sample = static_cast<int16_t>((static_cast<int>(data[i]) - 128) * 256);
            out_samples.push_back(sample);
        }
    } else { // 16-bit signed PCM
        size_t count = data_len / 2;
        out_samples.reserve(out_samples.size() + count);
        for (size_t i = 0; i < count; ++i) {
            int16_t sample = static_cast<int16_t>(data[i * 2] | (data[i * 2 + 1] << 8));
            out_samples.push_back(sample);
        }
    }
}

void AudioEngine::decode_adpcm(SWFStream& stream, size_t data_len, int channels, std::vector<int16_t>& out_samples) {
    if (data_len == 0) return;

    stream.align();
    uint8_t bits = static_cast<uint8_t>(stream.read_ub(2));
    uint8_t n_bits = bits + 2; // 2..5 bits

    const int* index_table = nullptr;
    switch (n_bits) {
        case 2: index_table = ADPCM_INDEX_TABLE_2; break;
        case 3: index_table = ADPCM_INDEX_TABLE_3; break;
        case 4: index_table = ADPCM_INDEX_TABLE_4; break;
        case 5: index_table = ADPCM_INDEX_TABLE_5; break;
        default: return;
    }

    struct ChannelState {
        int16_t valpred{0};
        int index{0};
    };

    std::vector<ChannelState> state(channels);
    for (int ch = 0; ch < channels; ++ch) {
        state[ch].valpred = static_cast<int16_t>(stream.read_sb(16));
        state[ch].index = static_cast<int>(stream.read_ub(6));
        state[ch].index = std::clamp(state[ch].index, 0, 88);
        out_samples.push_back(state[ch].valpred);
    }

    // Decode delta samples
    size_t start_offset = stream.tell();
    while (stream.bytes_remaining() > 0 && (stream.tell() - start_offset) <= data_len) {
        for (int ch = 0; ch < channels; ++ch) {
            if (stream.bytes_remaining() == 0) break;

            uint32_t delta = stream.read_ub(n_bits);
            int step = ADPCM_STEP_TABLE[state[ch].index];

            int diff = step >> (n_bits - 1);
            for (int b = 0; b < n_bits - 1; ++b) {
                if (delta & (1U << b)) {
                    diff += step >> ((n_bits - 2) - b);
                }
            }

            if (delta & (1U << (n_bits - 1))) {
                state[ch].valpred -= diff;
            } else {
                state[ch].valpred += diff;
            }

            state[ch].valpred = static_cast<int16_t>(std::clamp(static_cast<int>(state[ch].valpred), -32768, 32767));

            int mag = delta & ((1U << (n_bits - 1)) - 1);
            state[ch].index += index_table[mag];
            state[ch].index = std::clamp(state[ch].index, 0, 88);

            out_samples.push_back(state[ch].valpred);
        }
    }
}

bool AudioEngine::decode_sound_stream_block(SWFStream& stream, uint32_t tag_length, const SoundStreamHeader& header, std::vector<int16_t>& out_samples) {
    if (tag_length == 0 || !header.is_valid) return false;

    if (header.stream_sound_compression == SoundCompression::UncompressedNativeOrPCM ||
        header.stream_sound_compression == SoundCompression::UncompressedLittleEndian) {
        std::vector<uint8_t> block_data(tag_length);
        for (uint32_t i = 0; i < tag_length; ++i) {
            block_data[i] = stream.read_u8();
        }
        decode_raw_pcm(block_data.data(), block_data.size(), header.stream_sound_size, out_samples);
        return true;
    } else if (header.stream_sound_compression == SoundCompression::ADPCM) {
        decode_adpcm(stream, tag_length, header.channels(), out_samples);
        return true;
    }

    return false;
}

} // namespace reflash

#ifndef __EMSCRIPTEN__
extern "C" void reflash_js_play_audio(const int16_t* samples, int sample_count, int sample_rate, int channels) {
    (void)samples;
    (void)sample_count;
    (void)sample_rate;
    (void)channels;
}
#endif
