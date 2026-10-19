#include "reflash/swf_stream.hpp"
#include "reflash/swf_parser.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <cstring>
#include <zlib.h>

class BitWriter {
public:
    std::vector<uint8_t> buffer;
    uint8_t current_byte{0};
    uint8_t bit_count{0};

    void write_ub(uint32_t val, uint8_t n_bits) {
        for (int i = n_bits - 1; i >= 0; --i) {
            uint8_t bit = (val >> i) & 1;
            current_byte = (current_byte << 1) | bit;
            bit_count++;
            if (bit_count == 8) {
                buffer.push_back(current_byte);
                current_byte = 0;
                bit_count = 0;
            }
        }
    }

    void align() {
        if (bit_count > 0) {
            current_byte <<= (8 - bit_count);
            buffer.push_back(current_byte);
            current_byte = 0;
            bit_count = 0;
        }
    }
};

void test_bitstream_reader() {
    std::cout << "[Test] test_bitstream_reader..." << std::endl;

    std::vector<uint8_t> buffer = {0xB3, 0xCC, 0xF0};
    reflash::SWFStream stream(buffer);

    uint32_t ub1 = stream.read_ub(3);
    assert(ub1 == 5);

    int32_t sb1 = stream.read_sb(5);
    assert(sb1 == -13);

    assert(stream.tell() == 1);

    // Test RECT writing and reading using BitWriter
    BitWriter bw;
    bw.write_ub(12, 5); // n_bits = 12
    bw.write_ub(0, 12); // xmin = 0 twips
    bw.write_ub(1000, 12); // xmax = 1000 twips (50 px)
    bw.write_ub(0, 12); // ymin = 0 twips
    bw.write_ub(800, 12); // ymax = 800 twips (40 px)
    bw.align();

    reflash::SWFStream rect_stream(bw.buffer);
    reflash::SWFRect rect = rect_stream.read_rect();
    assert(rect.xmin == 0.0f);
    assert(rect.xmax == 50.0f);
    assert(rect.ymin == 0.0f);
    assert(rect.ymax == 40.0f);

    // Test primitives
    std::vector<uint8_t> prim_buf = {0x12, 0x34, 0x56, 0x78, 0x00, 0x01};
    reflash::SWFStream prim_stream(prim_buf);
    assert(prim_stream.read_u8() == 0x12);
    prim_stream.seek(0);
    assert(prim_stream.read_u16_le() == 0x3412);
    prim_stream.seek(0);
    assert(prim_stream.read_u32_le() == 0x78563412);

    std::vector<uint8_t> f8_buf = {0x80, 0x0C}; // 12.5f
    reflash::SWFStream f8_stream(f8_buf);
    assert(f8_stream.read_fixed8() == 12.5f);

    std::cout << "  -> test_bitstream_reader PASSED" << std::endl;
}

void test_uncompressed_fws_header() {
    std::cout << "[Test] test_uncompressed_fws_header..." << std::endl;

    std::vector<uint8_t> swf = {
        'F', 'W', 'S', 8,
        0, 0, 0, 0 // Placeholder for FileLength
    };

    // Construct RECT payload via BitWriter
    BitWriter bw;
    bw.write_ub(12, 5); // n_bits = 12
    bw.write_ub(0, 12); // xmin = 0
    bw.write_ub(1000, 12); // xmax = 1000 twips (50px)
    bw.write_ub(0, 12); // ymin = 0
    bw.write_ub(800, 12); // ymax = 800 twips (40px)
    bw.align();

    swf.insert(swf.end(), bw.buffer.begin(), bw.buffer.end());

    // FrameRate (12.0 FPS) = 0x0C00 LE: 0x00, 0x0C
    swf.push_back(0x00);
    swf.push_back(0x0C);

    // FrameCount (1) LE: 0x01, 0x00
    swf.push_back(0x01);
    swf.push_back(0x00);

    // End Tag (Tag 0, len 0) = 0x0000
    swf.push_back(0x00);
    swf.push_back(0x00);

    // Update length byte 4..7
    uint32_t total_len = static_cast<uint32_t>(swf.size());
    swf[4] = static_cast<uint8_t>(total_len & 0xFF);
    swf[5] = static_cast<uint8_t>((total_len >> 8) & 0xFF);
    swf[6] = static_cast<uint8_t>((total_len >> 16) & 0xFF);
    swf[7] = static_cast<uint8_t>((total_len >> 24) & 0xFF);

    reflash::SWFDocument doc;
    bool ok = doc.parse(swf.data(), swf.size());
    assert(ok);

    assert(doc.header().signature == reflash::SWFSignature::FWS);
    assert(doc.header().version == 8);
    assert(doc.header().frame_size.width() == 50.0f);
    assert(doc.header().frame_size.height() == 40.0f);
    assert(doc.header().frame_rate == 12.0f);
    assert(doc.header().frame_count == 1);

    std::cout << "  -> test_uncompressed_fws_header PASSED" << std::endl;
}

void test_compressed_cws_header() {
    std::cout << "[Test] test_compressed_cws_header..." << std::endl;

    BitWriter bw;
    bw.write_ub(12, 5);
    bw.write_ub(0, 12);
    bw.write_ub(1000, 12); // 50px
    bw.write_ub(0, 12);
    bw.write_ub(800, 12); // 40px
    bw.align();

    std::vector<uint8_t> payload = bw.buffer;
    // FrameRate = 24.0 FPS (0x1800 -> 0x00, 0x18)
    payload.push_back(0x00);
    payload.push_back(0x18);
    // FrameCount = 100 -> 0x0064 -> 0x64, 0x00
    payload.push_back(0x64);
    payload.push_back(0x00);
    // Tag 9 (SetBackgroundColor: #FF8800) -> TagCode 9, TagLen 3 -> (9 << 6) | 3 = 0x0243 -> LE: 0x43, 0x02
    payload.push_back(0x43);
    payload.push_back(0x02);
    payload.push_back(0xFF);
    payload.push_back(0x88);
    payload.push_back(0x00);
    // End Tag (0x0000)
    payload.push_back(0x00);
    payload.push_back(0x00);

    uLongf dest_len = compressBound(payload.size());
    std::vector<uint8_t> compressed_payload(dest_len);
    int compress_ret = compress(compressed_payload.data(), &dest_len, payload.data(), payload.size());
    assert(compress_ret == Z_OK);
    compressed_payload.resize(dest_len);

    uint32_t uncompressed_file_len = 8 + static_cast<uint32_t>(payload.size());
    std::vector<uint8_t> cws_file = {
        'C', 'W', 'S', 10,
        static_cast<uint8_t>(uncompressed_file_len & 0xFF),
        static_cast<uint8_t>((uncompressed_file_len >> 8) & 0xFF),
        static_cast<uint8_t>((uncompressed_file_len >> 16) & 0xFF),
        static_cast<uint8_t>((uncompressed_file_len >> 24) & 0xFF)
    };
    cws_file.insert(cws_file.end(), compressed_payload.begin(), compressed_payload.end());

    reflash::SWFDocument doc;
    bool ok = doc.parse(cws_file.data(), cws_file.size());
    assert(ok);

    assert(doc.header().signature == reflash::SWFSignature::CWS);
    assert(doc.header().version == 10);
    assert(doc.header().frame_size.width() == 50.0f);
    assert(doc.header().frame_size.height() == 40.0f);
    assert(doc.header().frame_rate == 24.0f);
    assert(doc.header().frame_count == 100);
    assert(doc.bg_color() == 0xFF8800);

    std::cout << "  -> test_compressed_cws_header PASSED" << std::endl;
}

void test_tag_parser_loop() {
    std::cout << "[Test] test_tag_parser_loop..." << std::endl;

    BitWriter bw;
    bw.write_ub(12, 5);
    bw.write_ub(0, 12);
    bw.write_ub(1000, 12);
    bw.write_ub(0, 12);
    bw.write_ub(800, 12);
    bw.align();

    std::vector<uint8_t> payload = bw.buffer;
    payload.push_back(0x00);
    payload.push_back(0x18);
    payload.push_back(0x01);
    payload.push_back(0x00);

    // Tag 69 (FileAttributes, AS3)
    payload.push_back(0x44);
    payload.push_back(0x11);
    payload.push_back(0x08); // AS3 flag set
    payload.push_back(0x00);
    payload.push_back(0x00);
    payload.push_back(0x00);

    // Tag 9 (SetBackgroundColor: #00FFCC)
    payload.push_back(0x43);
    payload.push_back(0x02);
    payload.push_back(0x00);
    payload.push_back(0xFF);
    payload.push_back(0xCC);

    // End Tag
    payload.push_back(0x00);
    payload.push_back(0x00);

    uint32_t total_len = 8 + static_cast<uint32_t>(payload.size());
    std::vector<uint8_t> swf = {
        'F', 'W', 'S', 9,
        static_cast<uint8_t>(total_len & 0xFF),
        static_cast<uint8_t>((total_len >> 8) & 0xFF),
        static_cast<uint8_t>((total_len >> 16) & 0xFF),
        static_cast<uint8_t>((total_len >> 24) & 0xFF)
    };
    swf.insert(swf.end(), payload.begin(), payload.end());

    reflash::SWFDocument doc;
    bool ok = doc.parse(swf.data(), swf.size());
    assert(ok);

    assert(doc.as_version() == 2); // AS3 / AVM2
    assert(doc.bg_color() == 0x00FFCC);
    assert(doc.tag_counts().at(69) == 1);
    assert(doc.tag_counts().at(9) == 1);

    std::cout << "  -> test_tag_parser_loop PASSED" << std::endl;
}

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "Running ReFlash Phase 2 SWF Parser Unit Tests" << std::endl;
    std::cout << "==========================================" << std::endl;

    test_bitstream_reader();
    test_uncompressed_fws_header();
    test_compressed_cws_header();
    test_tag_parser_loop();

    std::cout << "==========================================" << std::endl;
    std::cout << "All SWF Parser Unit Tests PASSED!" << std::endl;
    std::cout << "==========================================" << std::endl;
    return 0;
}
