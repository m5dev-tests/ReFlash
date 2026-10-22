#ifndef REFLASH_AUDIO_ENGINE_HPP
#define REFLASH_AUDIO_ENGINE_HPP

#include "reflash/swf_stream.hpp"
#include <cstdint>
#include <vector>
#include <cstddef>

namespace reflash {

enum class SoundCompression : uint8_t {
    UncompressedNativeOrPCM = 0, // 0 = Raw PCM (uncompressed)
    ADPCM = 1,                   // 1 = Flash ADPCM
    MP3 = 2,                     // 2 = MP3
    UncompressedLittleEndian = 3 // 3 = Uncompressed Little-Endian PCM
};


struct DefineSoundData {
    uint16_t sound_id{0};
    uint8_t sound_format{0}; // 2 = MP3
    uint8_t sound_rate{0};   // 0=5.5k, 1=11k, 2=22k, 3=44k
    uint8_t sound_size{0};   // 0=8bit, 1=16bit
    uint8_t sound_type{0};   // 0=mono, 1=stereo
    uint32_t sample_count{0};
    std::vector<uint8_t> sound_data;
};

struct StartSoundData {
    uint16_t sound_id{0};
    uint8_t sound_flags{0};
};

struct SoundStreamHeader {
    uint8_t playback_sound_rate{2}; // 0 = 5.5 kHz, 1 = 11.025 kHz, 2 = 22.05 kHz, 3 = 44.1 kHz
    uint8_t playback_sound_size{1}; // 0 = 8-bit, 1 = 16-bit
    uint8_t playback_sound_type{0}; // 0 = Mono, 1 = Stereo

    SoundCompression stream_sound_compression{SoundCompression::UncompressedNativeOrPCM};
    uint8_t stream_sound_rate{2}; // 0 = 5.5 kHz, 1 = 11.025 kHz, 2 = 22.05 kHz, 3 = 44.1 kHz
    uint8_t stream_sound_size{1}; // 0 = 8-bit, 1 = 16-bit
    uint8_t stream_sound_type{0}; // 0 = Mono, 1 = Stereo
    uint16_t stream_sound_sample_count{0};
    uint16_t latency_seek{0};     // Only present in SoundStreamHead2 if MP3 (format 2)
    bool is_valid{false};

    int sample_rate_hz() const;
    int channels() const { return stream_sound_type == 1 ? 2 : 1; }
};

class AudioEngine {
public:
    AudioEngine() = default;

    static SoundStreamHeader parse_sound_stream_head(SWFStream& stream, uint16_t tag_code, uint32_t tag_length);
    static bool decode_sound_stream_block(SWFStream& stream, uint32_t tag_length, const SoundStreamHeader& header, std::vector<int16_t>& out_samples);

    static void decode_raw_pcm(const uint8_t* data, size_t data_len, uint8_t sample_size, std::vector<int16_t>& out_samples);
    static void decode_adpcm(SWFStream& stream, size_t data_len, int channels, std::vector<int16_t>& out_samples);
};

} // namespace reflash

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define REFLASH_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define REFLASH_EXPORT
#endif

extern "C" {
REFLASH_EXPORT void reflash_js_play_audio(const int16_t* samples, int sample_count, int sample_rate, int channels);

REFLASH_EXPORT void reflash_js_register_mp3(int sound_id, const uint8_t* data, size_t size);
REFLASH_EXPORT void reflash_js_start_sound(int sound_id);

}

#endif // REFLASH_AUDIO_ENGINE_HPP
