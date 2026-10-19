#include "reflash/swf_parser.hpp"
#include "reflash/swf_stream.hpp"
#include <zlib.h>
#include <cstring>
#include <iostream>

namespace reflash {

static SWFDocument g_current_document;

bool SWFDocument::decompress_zlib(const uint8_t* src, size_t src_len, uint32_t expected_uncompressed_payload_len) {
    decompressed_buffer_.clear();
    // Re-construct the full decompressed file payload: 8 bytes uncompressed header + decompressed payload
    decompressed_buffer_.resize(8 + expected_uncompressed_payload_len);

    // Copy original 8-byte header into decompressed buffer and replace CWS signature with FWS
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
    bg_color_ = 0xFFFFFF;
    is_as3_ = false;

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
    header_.frame_rate = stream.read_fixed8();
    header_.frame_count = stream.read_u16_le();

    parse_tags(stream);
    return true;
}

void SWFDocument::parse_tags(SWFStream& stream) {
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
        tag_counts_[tag_code]++;

        // Parse specific tags of interest
        if (tag_code == 0) { // End Tag
            break;
        } else if (tag_code == 9) { // SetBackgroundColor
            if (tag_length >= 3) {
                uint8_t r = stream.read_u8();
                uint8_t g = stream.read_u8();
                uint8_t b = stream.read_u8();
                bg_color_ = (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
            }
        } else if (tag_code == 69) { // FileAttributes
            if (tag_length >= 1) {
                uint8_t flags = stream.read_u8();
                is_as3_ = (flags & 0x08) != 0; // Bit 3: ActionScript 3.0 flag
            }
        } else if (tag_code == 72) { // DoABC
            is_as3_ = true;
        }

        // Always position stream at next tag start to guarantee stream alignment
        stream.seek(tag_next_offset);
    }
}

} // namespace reflash

extern "C" {

REFLASH_EXPORT int reflash_load_swf(const uint8_t* buffer, size_t length) {
    return reflash::g_current_document.parse(buffer, length) ? 1 : 0;
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

}
