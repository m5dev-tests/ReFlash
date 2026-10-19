#ifndef REFLASH_SWF_PARSER_HPP
#define REFLASH_SWF_PARSER_HPP

#include "reflash/swf_stream.hpp"
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

namespace reflash {

enum class SWFSignature {
    FWS, // Uncompressed
    CWS, // Zlib compressed
    ZWS, // LZMA compressed (unsupported)
    Unknown
};

struct PlaceObject2Data {
    bool flag_move{false};
    bool has_character{false};
    bool has_matrix{false};
    bool has_color_transform{false};
    bool has_ratio{false};
    bool has_name{false};
    bool has_clip_depth{false};

    uint16_t depth{0};
    uint16_t character_id{0};
    uint16_t ratio{0};
    uint16_t clip_depth{0};
    std::string name;

    // Matrix parameters
    float scale_x{1.0f}, scale_y{1.0f};
    float rotate_skew0{0.0f}, rotate_skew1{0.0f};
    float translate_x{0.0f}, translate_y{0.0f};

    // Color transform with alpha
    float r_mult{1.0f}, g_mult{1.0f}, b_mult{1.0f}, a_mult{1.0f};
    float r_add{0.0f}, g_add{0.0f}, b_add{0.0f}, a_add{0.0f};
};

struct PlaceObject3Data : public PlaceObject2Data {
    uint8_t blend_mode{1}; // 1 = Normal
};

struct FileAttributesData {
    bool use_direct_blit{false};
    bool use_gpu{false};
    bool has_metadata{false};
    bool actionscript3{false}; // bit 3: 1 for AS3, 0 for AS2
    bool use_network{false};
};

struct DoActionData {
    std::vector<uint8_t> bytecode;
};

struct DoABCData {
    uint32_t flags{0};
    std::string name;
    std::vector<uint8_t> bytecode;
};

struct SWFTag {
    uint16_t code{0};
    uint32_t length{0};
    size_t header_offset{0};
    size_t payload_offset{0};
};

struct SWFHeader {
    SWFSignature signature{SWFSignature::Unknown};
    uint8_t version{0};
    uint32_t file_length{0};
    SWFRect frame_size;
    float frame_rate{0.0f};
    uint16_t frame_count{0};
};

class SWFDocument {
public:
    SWFDocument() = default;

    bool parse(const uint8_t* buffer, size_t size);

    const SWFHeader& header() const { return header_; }
    uint32_t bg_color() const { return bg_color_; }
    uint8_t as_version() const { return is_as3_ ? 2 : 1; }
    bool is_as3() const { return is_as3_; }

    const std::vector<SWFTag>& tags() const { return tags_; }
    const std::unordered_map<uint16_t, uint32_t>& tag_counts() const { return tag_counts_; }

private:
    SWFHeader header_;
    uint32_t bg_color_{0xFFFFFF}; // 0xRRGGBB
    bool is_as3_{false}; // Default to AS1/2 (AVM1)
    std::vector<uint8_t> decompressed_buffer_;

    std::vector<SWFTag> tags_;
    std::unordered_map<uint16_t, uint32_t> tag_counts_;

    bool decompress_zlib(const uint8_t* src, size_t src_len, uint32_t expected_uncompressed_len);
    void parse_tags(SWFStream& stream);
};

} // namespace reflash

// C-linkage exported APIs for Wasm / Web interface
extern "C" {
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define REFLASH_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define REFLASH_EXPORT
#endif

REFLASH_EXPORT int reflash_load_swf(const uint8_t* buffer, size_t length);
REFLASH_EXPORT float reflash_get_width();
REFLASH_EXPORT float reflash_get_height();
REFLASH_EXPORT float reflash_get_framerate();
REFLASH_EXPORT int reflash_get_frame_count();
REFLASH_EXPORT int reflash_get_version();
REFLASH_EXPORT int reflash_get_as_version();
REFLASH_EXPORT uint32_t reflash_get_bg_color();

}

#endif // REFLASH_SWF_PARSER_HPP
