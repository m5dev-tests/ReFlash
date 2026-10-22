#include "reflash/audio_engine.hpp"
#include "reflash/swf_stream.hpp"
#include <cassert>
#include <iostream>
#include <vector>

static std::vector<uint8_t> g_registered_mp3;
static int g_registered_sound_id = -1;

void test_register_mp3(int sound_id, const uint8_t* data, size_t size) {
    g_registered_sound_id = sound_id;
    g_registered_mp3.assign(data, data + size);
}

void test_sound_stream_head_unpacking() {
    // Construct Tag 18 payload:
    // b0: reserved (0), PlaybackSoundRate=2 (22kHz), PlaybackSoundSize=1 (16-bit), PlaybackSoundType=1 (Stereo)
    // b0 = (2 << 2) | (1 << 1) | 1 = 8 | 2 | 1 = 11 (0x0B)
    // b1: StreamSoundCompression=1 (ADPCM), StreamSoundRate=3 (44kHz), StreamSoundSize=1 (16-bit), StreamSoundType=0 (Mono)
    // b1 = (1 << 4) | (3 << 2) | (1 << 1) | 0 = 16 | 12 | 2 | 0 = 30 (0x1E)
    // sample_count = 512 (0x0200)
    std::vector<uint8_t> data = { 0x0B, 0x1E, 0x00, 0x02 };
    reflash::SWFStream stream(data);

    reflash::SoundStreamHeader hdr = reflash::AudioEngine::parse_sound_stream_head(stream, 18, static_cast<uint32_t>(data.size()));

    assert(hdr.is_valid);
    assert(hdr.playback_sound_rate == 2);
    assert(hdr.playback_sound_size == 1);
    assert(hdr.playback_sound_type == 1);

    assert(hdr.stream_sound_compression == reflash::SoundCompression::ADPCM);
    assert(hdr.stream_sound_rate == 3);
    assert(hdr.stream_sound_size == 1);
    assert(hdr.stream_sound_type == 0);
    assert(hdr.stream_sound_sample_count == 512);
    assert(hdr.sample_rate_hz() == 44100);
    assert(hdr.channels() == 1);

    std::cout << "test_sound_stream_head_unpacking passed!" << std::endl;
}

void test_raw_pcm_decoding() {
    // 8-bit PCM test
    std::vector<uint8_t> pcm8 = { 0, 128, 255 }; // min, zero, max
    std::vector<int16_t> samples8;
    reflash::AudioEngine::decode_raw_pcm(pcm8.data(), pcm8.size(), 0, samples8);

    assert(samples8.size() == 3);
    assert(samples8[0] == -32768);
    assert(samples8[1] == 0);
    assert(samples8[2] == 32512);

    // 16-bit PCM test
    std::vector<uint8_t> pcm16 = { 0x00, 0x00, 0x00, 0x80, 0xFF, 0x7F }; // 0, -32768, 32767
    std::vector<int16_t> samples16;
    reflash::AudioEngine::decode_raw_pcm(pcm16.data(), pcm16.size(), 1, samples16);

    assert(samples16.size() == 3);
    assert(samples16[0] == 0);
    assert(samples16[1] == -32768);
    assert(samples16[2] == 32767);

    std::cout << "test_raw_pcm_decoding passed!" << std::endl;
}

void test_adpcm_decoding() {
    // Simple ADPCM bitstream:
    // 2 bits for nBits code (0 -> 2 bits per delta)
    // Mono: 16 bits initial sample (e.g., 1000 = 0x03E8), 6 bits initial index (0)
    // Bit stream construction:
    // bits: 2 bits = 00b
    // valpred: 16 bits = 0x0064 (100)
    // index: 6 bits = 0
    std::vector<uint8_t> adpcm_data = {
        0x00, 0x01, 0x90, 0x00, 0x00
    };
    reflash::SWFStream stream(adpcm_data);
    std::vector<int16_t> samples;
    reflash::AudioEngine::decode_adpcm(stream, adpcm_data.size(), 1, samples);

    assert(!samples.empty());
    std::cout << "test_adpcm_decoding passed! Decoded " << samples.size() << " samples." << std::endl;
}

void test_mp3_event_sound_seek_samples_strip() {
    std::cout << "Running test_mp3_event_sound_seek_samples_strip..." << std::endl;

    // Build Tag 14 (DefineSound) payload:
    // SoundId = 100 (0x0064)
    // SoundFormat = 2 (MP3), SoundRate = 3 (44kHz), SoundSize = 1 (16bit), SoundType = 1 (Stereo) -> flags = (2<<4)|(3<<2)|(1<<1)|1 = 0x2F
    // SampleCount = 1000 (0x000003E8)
    // SeekSamples = 24 (0x0018)
    // Payload bytes: 0x11, 0x22, 0x33, 0x44
    std::vector<uint8_t> payload;
    payload.push_back(0x64); payload.push_back(0x00); // SoundId
    payload.push_back(0x2F);                          // Flags
    payload.push_back(0xE8); payload.push_back(0x03); payload.push_back(0x00); payload.push_back(0x00); // SampleCount
    payload.push_back(0x18); payload.push_back(0x00); // SeekSamples = 24
    payload.push_back(0x11); payload.push_back(0x22); payload.push_back(0x33); payload.push_back(0x44); // Raw MP3

    uint32_t tag_len = static_cast<uint32_t>(payload.size());
    reflash::SWFStream stream(payload);

    uint16_t sound_id = stream.read_u16_le();
    uint8_t flags = stream.read_u8();
    uint8_t sound_format = (flags >> 4) & 0x0F;
    uint32_t sample_count = stream.read_u32_le();

    assert(sound_id == 100);
    assert(sound_format == 2);
    assert(sample_count == 1000);

    size_t header_len = 2 + 1 + 4;
    size_t payload_size = tag_len - header_len;
    uint16_t seek_samples = stream.read_u16_le();
    assert(seek_samples == 24);

    size_t mp3_size = payload_size - 2;
    std::vector<uint8_t> mp3_bytes(mp3_size);
    for (size_t i = 0; i < mp3_size; ++i) {
        mp3_bytes[i] = stream.read_u8();
    }

    test_register_mp3(sound_id, mp3_bytes.data(), mp3_bytes.size());

    assert(g_registered_sound_id == 100);
    assert(g_registered_mp3.size() == 4);
    assert(g_registered_mp3[0] == 0x11);
    assert(g_registered_mp3[1] == 0x22);
    assert(g_registered_mp3[2] == 0x33);
    assert(g_registered_mp3[3] == 0x44);

    std::cout << "test_mp3_event_sound_seek_samples_strip passed!" << std::endl;
}

int main() {
    std::cout << "Running ReFlash Audio Engine Unit Tests..." << std::endl;
    test_sound_stream_head_unpacking();
    test_raw_pcm_decoding();
    test_adpcm_decoding();
    test_mp3_event_sound_seek_samples_strip();
    std::cout << "All Audio Engine Unit Tests Passed!" << std::endl;
    return 0;
}
