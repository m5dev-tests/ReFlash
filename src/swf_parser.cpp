#include "reflash/swf_parser.hpp"
#include "reflash/swf_stream.hpp"
#include "reflash/avm2_context.hpp"
#include "reflash/audio_engine.hpp"
#include <zlib.h>
#include <cstring>
#include <fstream>
#include <iostream>

namespace reflash {

static SWFDocument g_current_document;

SWFDocument& get_current_document() {
    return g_current_document;
}

bool load_movie_from_vfs(const std::string& url, std::vector<uint8_t>& out_buffer) {
    if (url.empty()) return false;
    out_buffer.clear();

    std::vector<std::string> paths;
    if (url[0] != '/') {
        paths.push_back("/" + url);
    }
    paths.push_back(url);

    for (const auto& path : paths) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            std::streamsize size = file.tellg();
            if (size > 0) {
                file.seekg(0, std::ios::beg);
                out_buffer.resize(static_cast<size_t>(size));
                if (file.read(reinterpret_cast<char*>(out_buffer.data()), size)) {
                    return true;
                }
            }
        }
    }
    return false;
}

uint32_t SWFDocument::frame_interval_ms() const {
    // Pixel-Perfect Frame Timing Formula using Adobe internal fixed-point representation
    if (header_.frame_rate_fixed8 > 0) {
        return 256000 / header_.frame_rate_fixed8;
    }
    return 1000 / 24;
}

bool SWFDocument::decompress_zlib(const uint8_t* src, size_t src_len, uint32_t expected_uncompressed_payload_len) {
    decompressed_buffer_.clear();
    // Re-construct the full decompressed file payload: 8 bytes uncompressed header + decompressed payload
    decompressed_buffer_.resize(8 + expected_uncompressed_payload_len);

    // Copy original 8-byte header into decompressed buffer and overwrite byte 0: 'C' -> 'F'
    std::memcpy(decompressed_buffer_.data(), src, 8);
    decompressed_buffer_[0] = 'F';

    z_stream strm;
    std::memset(&strm, 0, sizeof(strm));
    strm.next_in = const_cast<Bytef*>(src + 8);
    strm.avail_in = static_cast<uInt>(src_len - 8);
    strm.next_out = reinterpret_cast<Bytef*>(decompressed_buffer_.data() + 8);
    strm.avail_out = static_cast<uInt>(expected_uncompressed_payload_len);

    if (inflateInit(&strm) != Z_OK) {
        return false;
    }

    int ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);

    if (ret != Z_STREAM_END && ret != Z_OK) {
        return false;
    }

    return true;
}

bool SWFDocument::parse(const uint8_t* buffer, size_t size) {
    if (!buffer || size < 8) return false;

    tags_.clear();
    tag_counts_.clear();
    asset_dictionary_.clear();
    jpeg_assets_.clear();
    sprites_.clear();
    symbol_classes_.clear();
    timeline_frames_.clear();
    bg_color_ = 0xFFFFFF;
    is_as3_ = false;
    sound_stream_header_ = SoundStreamHeader();

    // Read signature
    char sig0 = static_cast<char>(buffer[0]);
    char sig1 = static_cast<char>(buffer[1]);
    char sig2 = static_cast<char>(buffer[2]);

    if (sig1 != 'W' || sig2 != 'S') {
        return false;
    }

    if (sig0 == 'F') {
        header_.signature = SWFSignature::FWS;
    } else if (sig0 == 'C') {
        header_.signature = SWFSignature::CWS;
    } else if (sig0 == 'Z') {
        header_.signature = SWFSignature::ZWS;
        std::cerr << "SWF LZMA compression (ZWS) is unsupported." << std::endl;
        return false;
    } else {
        return false;
    }

    header_.version = buffer[3];
    header_.file_length = static_cast<uint32_t>(buffer[4] | (buffer[5] << 8) | (buffer[6] << 16) | (buffer[7] << 24));

    const uint8_t* parse_buffer = buffer;
    size_t parse_size = size;

    if (header_.signature == SWFSignature::CWS) {
        if (header_.file_length < 8) return false;
        uint32_t expected_payload_len = header_.file_length - 8;
        if (!decompress_zlib(buffer, size, expected_payload_len)) {
            std::cerr << "SWF Zlib decompression failed." << std::endl;
            return false;
        }
        parse_buffer = decompressed_buffer_.data();
        parse_size = decompressed_buffer_.size();
    }

    SWFStream stream(parse_buffer, parse_size);
    // Skip 8-byte header (signature, version, file length)
    stream.seek(8);

    header_.frame_size = stream.read_rect();

    // Store 8.8 fixed-point frame rate value before conversion to float
    size_t fr_offset = stream.tell();
    uint8_t fr_b0 = parse_buffer[fr_offset];
    uint8_t fr_b1 = parse_buffer[fr_offset + 1];
    header_.frame_rate_fixed8 = static_cast<uint16_t>(fr_b0 | (fr_b1 << 8));

    header_.frame_rate = stream.read_fixed8();
    header_.frame_count = stream.read_u16_le();

    parse_tags(stream);
    return true;
}

void SWFDocument::parse_tags(SWFStream& stream) {
    TimelineFrame current_frame;
    current_frame.frame_index = 0;

    while (stream.bytes_remaining() >= 2) {
        size_t tag_header_offset = stream.tell();
        uint16_t tag_code_and_length = stream.read_u16_le();
        uint16_t tag_code = tag_code_and_length >> 6;
        uint32_t tag_length = tag_code_and_length & 0x3F;

        if (tag_length == 0x3F) {
            if (stream.bytes_remaining() < 4) break;
            tag_length = stream.read_u32_le();
        }

        size_t tag_payload_offset = stream.tell();
        size_t tag_next_offset = tag_payload_offset + tag_length;

        SWFTag tag;
        tag.code = tag_code;
        tag.length = tag_length;
        tag.header_offset = tag_header_offset;
        tag.payload_offset = tag_payload_offset;

        tags_.push_back(tag);

        // Parse-time registration of definition tags into Global Asset Dictionary
        if (tag_code == 2 || tag_code == 22 || tag_code == 32 || tag_code == 83 || // DefineShape
            tag_code == 14 || // DefineSound
            tag_code == 21 || tag_code == 35 || // DefineBitsJPEG2 / DefineBitsJPEG3
            tag_code == 20 || tag_code == 36 || // DefineBitsLossless / DefineBitsLossless2
            tag_code == 10 || tag_code == 48 || tag_code == 62 || // DefineFont / DefineFont2 / DefineFont3
            tag_code == 39) { // DefineSprite

            if (tag_length >= 2) {
                uint16_t char_id = stream.read_u16_le();
                SWFAsset asset;
                asset.character_id = char_id;
                asset.tag_code = tag_code;
                asset.payload.resize(tag_length);
                // Rewind 2 bytes for character ID
                stream.seek(tag_payload_offset);
                for (size_t i = 0; i < tag_length; ++i) {
                    asset.payload[i] = stream.read_u8();
                }
                asset_dictionary_[char_id] = asset;
                // Reset stream to tag_payload_offset for tag specific parsers
                stream.seek(tag_payload_offset);
            }
        }

        tag_counts_[tag_code]++;

        // Tag Handling according to Open-Flash spec & Engine Quirks
        if (tag_code == 0) { // Tag 0: End
            break;
        } else if (tag_code == 1) { // Tag 1: ShowFrame -> Flush deferred timeline commands for frame
            timeline_frames_.push_back(current_frame);
            current_frame.frame_index++;
            current_frame.commands.clear();
            current_frame.audio_samples.clear();
                } else if (tag_code == 14) { // Tag 14: DefineSound
            if (tag_length >= 7) {
                uint16_t sound_id = stream.read_u16_le();
                uint8_t flags = stream.read_u8();
                uint8_t sound_format = (flags >> 4) & 0x0F;
                uint8_t sound_rate = (flags >> 2) & 0x03;
                uint8_t sound_size = (flags >> 1) & 0x01;
                uint8_t sound_type = flags & 0x01;
                uint32_t sample_count = stream.read_u32_le();

                size_t header_len = 2 + 1 + 4;
                if (tag_length > header_len) {
                    size_t payload_size = tag_length - header_len;
                    if (sound_format == 2) { // MP3
                        if (payload_size > 2) {
                            uint16_t seek_samples = stream.read_u16_le(); // SI16 SeekSamples
                            (void)seek_samples;
                            size_t mp3_size = payload_size - 2;
                            std::vector<uint8_t> mp3_bytes(mp3_size);
                            for (size_t i = 0; i < mp3_size; ++i) {
                                mp3_bytes[i] = stream.read_u8();
                            }
                            reflash_js_register_mp3(sound_id, mp3_bytes.data(), mp3_bytes.size());
                        }
                    }
                }
            }
        } else if (tag_code == 15) { // Tag 15: StartSound
            if (tag_length >= 2) {
                uint16_t sound_id = stream.read_u16_le();
                if (tag_length >= 3) {
                    uint8_t sound_flags = stream.read_u8();
                    (void)sound_flags;
                }
                reflash_js_start_sound(sound_id);
            }
        } else if (tag_code == 18 || tag_code == 45) { // Tag 18 / Tag 45: SoundStreamHead / SoundStreamHead2
            sound_stream_header_ = AudioEngine::parse_sound_stream_head(stream, tag_code, tag_length);
        } else if (tag_code == 19) { // Tag 19: SoundStreamBlock
            if (sound_stream_header_.is_valid) {
                AudioEngine::decode_sound_stream_block(stream, tag_length, sound_stream_header_, current_frame.audio_samples);
            }
        } else if (tag_code == 9) { // Tag 9: SetBackgroundColor
            if (tag_length >= 3) {
                uint8_t r = stream.read_u8();
                uint8_t g = stream.read_u8();
                uint8_t b = stream.read_u8();
                bg_color_ = (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
            }
        } else if (tag_code == 69) { // Tag 69: FileAttributes
            if (tag_length >= 1) {
                uint8_t flags = stream.read_u8();
                is_as3_ = (flags & 0x08) != 0; // Bit 3: ActionScript 3.0 flag
            }
        } else if (tag_code == 72 || tag_code == 82) { // Tag 72/82: DoABC
            if (is_as3_) {
                uint32_t flags = stream.read_u32_le();
                std::string abc_name = stream.read_string();
                size_t header_bytes_read = sizeof(uint32_t) + abc_name.length() + 1;

                if (tag_length > header_bytes_read) {
                    size_t abc_bytecode_len = tag_length - header_bytes_read;
                    std::vector<uint8_t> abc_bytecode(abc_bytecode_len);
                    for (size_t i = 0; i < abc_bytecode_len; ++i) {
                        abc_bytecode[i] = stream.read_u8();
                    }
                    reflash_execute_abc(abc_bytecode.data(), abc_bytecode.size(), abc_name.c_str());
                }
            }
        } else if (tag_code == 21) { // Tag 21: DefineBitsJPEG2
            if (tag_length >= 2) {
                uint16_t char_id = stream.read_u16_le();
                JPEGAsset asset;
                asset.character_id = char_id;
                if (tag_length > 2) {
                    size_t img_len = tag_length - 2;
                    asset.image_data.resize(img_len);
                    for (size_t i = 0; i < img_len; ++i) {
                        asset.image_data[i] = stream.read_u8();
                    }
                }
                jpeg_assets_[char_id] = asset;
            }
        } else if (tag_code == 35) { // Tag 35: DefineBitsJPEG3
            if (tag_length >= 6) {
                uint16_t char_id = stream.read_u16_le();
                uint32_t alpha_data_offset = stream.read_u32_le();
                JPEGAsset asset;
                asset.character_id = char_id;

                if (alpha_data_offset > 0 && alpha_data_offset < tag_length - 6) {
                    asset.image_data.resize(alpha_data_offset);
                    for (size_t i = 0; i < alpha_data_offset; ++i) {
                        asset.image_data[i] = stream.read_u8();
                    }
                    size_t alpha_len = (tag_length - 6) - alpha_data_offset;
                    asset.alpha_data.resize(alpha_len);
                    for (size_t i = 0; i < alpha_len; ++i) {
                        asset.alpha_data[i] = stream.read_u8();
                    }
                }
                jpeg_assets_[char_id] = asset;
            }
        } else if (tag_code == 39) { // Tag 39: DefineSprite
            if (tag_length >= 4) {
                uint16_t sprite_id = stream.read_u16_le();
                uint16_t frame_count = stream.read_u16_le();
                DefineSpriteData sprite;
                sprite.sprite_id = sprite_id;
                sprite.frame_count = frame_count;
                sprites_[sprite_id] = sprite;
            }
        } else if (tag_code == 76) { // Tag 76: SymbolClass
            if (tag_length >= 2) {
                uint16_t num_symbols = stream.read_u16_le();
                for (uint16_t i = 0; i < num_symbols; ++i) {
                    if (stream.tell() >= tag_next_offset) break;
                    uint16_t char_id = stream.read_u16_le();
                    std::string class_name = stream.read_string();
                    symbol_classes_[char_id] = class_name;
                }
            }
        } else if (tag_code == 26 || tag_code == 70) { // PlaceObject2 / PlaceObject3
            FrameCommand cmd;
            cmd.type = FrameCommandType::PlaceObject;
            if (tag_length >= 3) {
                uint8_t flags1 = stream.read_u8();
                uint16_t depth = stream.read_u16_le();
                cmd.depth = depth;
                cmd.place_data.depth = depth;

                bool has_character = (flags1 & 0x02) != 0;
                if (has_character) {
                    uint16_t char_id = stream.read_u16_le();
                    cmd.character_id = char_id;
                    cmd.place_data.character_id = char_id;
                    cmd.place_data.has_character = true;
                }
            }
            current_frame.commands.push_back(cmd);
        } else if (tag_code == 28) { // RemoveObject2
            if (tag_length >= 2) {
                uint16_t depth = stream.read_u16_le();
                FrameCommand cmd;
                cmd.type = FrameCommandType::RemoveObject;
                cmd.depth = depth;
                current_frame.commands.push_back(cmd);
            }
        } else if (tag_code == 12) { // DoAction
            FrameCommand cmd;
            cmd.type = FrameCommandType::DoAction;
            if (tag_length > 0) {
                cmd.action_bytecode.resize(tag_length);
                for (size_t i = 0; i < tag_length; ++i) {
                    cmd.action_bytecode[i] = stream.read_u8();
                }
            }
            current_frame.commands.push_back(cmd);
        }

        // Guarantee stream alignment for next tag
        stream.seek(tag_next_offset);
    }

    if (!current_frame.commands.empty() || !current_frame.audio_samples.empty()) {
        timeline_frames_.push_back(current_frame);
    }
}

} // namespace reflash

extern "C" {

REFLASH_EXPORT int reflash_load_swf(const uint8_t* buffer, size_t length) {
    return reflash::g_current_document.parse(buffer, length) ? 1 : 0;
}

REFLASH_EXPORT int reflash_load_movie(const char* url) {
    if (!url) return 0;
    std::vector<uint8_t> buffer;
    if (reflash::load_movie_from_vfs(url, buffer)) {
        return reflash::g_current_document.parse(buffer.data(), buffer.size()) ? 1 : 0;
    }
    return 0;
}

REFLASH_EXPORT float reflash_get_width() {
    return reflash::g_current_document.header().frame_size.width();
}

REFLASH_EXPORT float reflash_get_height() {
    return reflash::g_current_document.header().frame_size.height();
}

REFLASH_EXPORT float reflash_get_framerate() {
    return reflash::g_current_document.header().frame_rate;
}

REFLASH_EXPORT int reflash_get_frame_count() {
    return reflash::g_current_document.header().frame_count;
}

REFLASH_EXPORT int reflash_get_version() {
    return reflash::g_current_document.header().version;
}

REFLASH_EXPORT int reflash_get_as_version() {
    return reflash::g_current_document.as_version();
}

REFLASH_EXPORT uint32_t reflash_get_bg_color() {
    return reflash::g_current_document.bg_color();
}

REFLASH_EXPORT uint32_t reflash_get_frame_interval_ms() {
    return reflash::g_current_document.frame_interval_ms();
}

}


REFLASH_EXPORT int reflash_has_asset(uint16_t character_id) {
    return reflash::g_current_document.has_asset(character_id) ? 1 : 0;
}

REFLASH_EXPORT int reflash_get_asset_count() {
    return static_cast<int>(reflash::g_current_document.asset_dictionary().size());
}

namespace reflash {

ActionExecutionResult execute_avm1_action(const std::vector<uint8_t>& bytecode, uint32_t total_frames) {
    ActionExecutionResult res;
    if (bytecode.empty()) return res;

    SWFStream stream(bytecode);
    size_t actions_skipped = 0;

    while (stream.bytes_remaining() > 0) {
        uint8_t opcode = stream.read_u8();
        if (opcode == 0x00) { // ActionEnd
            break;
        }

        uint16_t length = 0;
        if (opcode >= 0x80) {
            if (stream.bytes_remaining() < 2) break;
            length = stream.read_u16_le();
        }

        if (stream.bytes_remaining() < length) break;

        size_t action_body_offset = stream.tell();

        if (opcode == 0x8A) { // ActionWaitForFrame
            if (length >= 3) {
                uint16_t target_frame = stream.read_u16_le();
                uint8_t skip_count = stream.read_u8();

                // Check condition: frame is loaded
                if (target_frame < total_frames) {
                    res.skipped_actions = true;
                    res.skip_count = skip_count;

                    // Skip the next skip_count actions in the stream
                    actions_skipped = 0;
                    while (actions_skipped < skip_count && stream.bytes_remaining() > 0) {
                        uint8_t next_op = stream.read_u8();
                        if (next_op == 0x00) break;
                        uint16_t next_len = 0;
                        if (next_op >= 0x80) {
                            if (stream.bytes_remaining() < 2) break;
                            next_len = stream.read_u16_le();
                        }
                        if (stream.bytes_remaining() < next_len) break;
                        stream.seek(stream.tell() + next_len);
                        actions_skipped++;
                    }
                    continue;
                }
            }
        } else if (opcode == 0x81) { // ActionGotoFrame
            if (length >= 2) {
                res.goto_frame = true;
                res.goto_frame_target = stream.read_u16_le();
            }
        } else if (opcode == 0x06) { // ActionPlay
            res.play_requested = true;
        } else if (opcode == 0x07) { // ActionStop
            res.stop_requested = true;
        }

        stream.seek(action_body_offset + length);
    }

    return res;
}

}
