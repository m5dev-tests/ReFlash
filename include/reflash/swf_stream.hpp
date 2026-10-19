#ifndef REFLASH_SWF_STREAM_HPP
#define REFLASH_SWF_STREAM_HPP

#include <cstdint>
#include <cstddef>
#include <vector>
#include <stdexcept>

namespace reflash {

struct SWFRect {
    float xmin{0.0f}; // in pixels
    float xmax{0.0f}; // in pixels
    float ymin{0.0f}; // in pixels
    float ymax{0.0f}; // in pixels

    float width() const { return xmax - xmin; }
    float height() const { return ymax - ymin; }
};

class SWFStream {
public:
    SWFStream() = default;
    SWFStream(const uint8_t* data, size_t size);
    explicit SWFStream(const std::vector<uint8_t>& buffer);

    void align();

    uint32_t read_ub(uint8_t n_bits);
    int32_t read_sb(uint8_t n_bits);
    float read_fb(uint8_t n_bits);

    uint8_t read_u8();
    uint16_t read_u16_le();
    uint32_t read_u32_le();
    float read_fixed8();

    SWFRect read_rect();

    size_t tell() const { return byte_offset_; }
    void seek(size_t byte_offset);
    size_t bytes_remaining() const;

private:
    const uint8_t* data_{nullptr};
    size_t size_{0};
    size_t byte_offset_{0};
    uint8_t bit_offset_{0}; // 0..7 (0 means aligned to MSB of current byte)
};

} // namespace reflash

#endif // REFLASH_SWF_STREAM_HPP
