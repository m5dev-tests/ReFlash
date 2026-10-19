#include "reflash/swf_stream.hpp"

namespace reflash {

SWFStream::SWFStream(const uint8_t* data, size_t size)
    : data_(data), size_(size), byte_offset_(0), bit_offset_(0) {}

SWFStream::SWFStream(const std::vector<uint8_t>& buffer)
    : data_(buffer.data()), size_(buffer.size()), byte_offset_(0), bit_offset_(0) {}

void SWFStream::align() {
    if (bit_offset_ != 0) {
        bit_offset_ = 0;
        byte_offset_++;
    }
}

uint32_t SWFStream::read_ub(uint8_t n_bits) {
    if (n_bits == 0) return 0;
    if (n_bits > 32) throw std::out_of_range("read_ub: bit depth exceeds 32 bits");

    uint32_t value = 0;
    uint8_t bits_needed = n_bits;

    while (bits_needed > 0) {
        if (byte_offset_ >= size_) {
            throw std::out_of_range("SWFStream read past end of buffer");
        }

        uint8_t bits_available = 8 - bit_offset_;
        uint8_t bits_to_read = (bits_needed < bits_available) ? bits_needed : bits_available;

        // Shift existing accumulated value left to make room for new bits
        value <<= bits_to_read;

        // Extract bits from current byte starting at bit_offset_ from MSB
        uint8_t current_byte = data_[byte_offset_];
        uint8_t shift = bits_available - bits_to_read;
        uint8_t mask = (1 << bits_to_read) - 1;
        uint8_t extracted = (current_byte >> shift) & mask;

        value |= extracted;

        bit_offset_ += bits_to_read;
        bits_needed -= bits_to_read;

        if (bit_offset_ == 8) {
            bit_offset_ = 0;
            byte_offset_++;
        }
    }

    return value;
}

int32_t SWFStream::read_sb(uint8_t n_bits) {
    if (n_bits == 0) return 0;
    uint32_t raw = read_ub(n_bits);

    // Sign extend if MSB of n_bits is set
    if (raw & (1U << (n_bits - 1))) {
        uint32_t mask = (n_bits >= 32) ? 0 : (~0U << n_bits);
        raw |= mask;
    }
    return static_cast<int32_t>(raw);
}

float SWFStream::read_fb(uint8_t n_bits) {
    if (n_bits == 0) return 0.0f;
    int32_t raw = read_sb(n_bits);
    return static_cast<float>(raw) / 65536.0f;
}

uint8_t SWFStream::read_u8() {
    align();
    if (byte_offset_ >= size_) {
        throw std::out_of_range("SWFStream read past end of buffer");
    }
    return data_[byte_offset_++];
}

uint16_t SWFStream::read_u16_le() {
    uint8_t b0 = read_u8();
    uint8_t b1 = read_u8();
    return static_cast<uint16_t>(b0 | (b1 << 8));
}

uint32_t SWFStream::read_u32_le() {
    uint8_t b0 = read_u8();
    uint8_t b1 = read_u8();
    uint8_t b2 = read_u8();
    uint8_t b3 = read_u8();
    return static_cast<uint32_t>(b0 | (b1 << 8) | (b2 << 16) | (b3 << 24));
}

float SWFStream::read_fixed8() {
    uint16_t raw = read_u16_le();
    int16_t signed_raw = static_cast<int16_t>(raw);
    return static_cast<float>(signed_raw) / 256.0f;
}

SWFRect SWFStream::read_rect() {
    align();
    uint8_t n_bits = static_cast<uint8_t>(read_ub(5));
    int32_t xmin_twips = read_sb(n_bits);
    int32_t xmax_twips = read_sb(n_bits);
    int32_t ymin_twips = read_sb(n_bits);
    int32_t ymax_twips = read_sb(n_bits);
    align();

    SWFRect rect;
    rect.xmin = static_cast<float>(xmin_twips) / 20.0f;
    rect.xmax = static_cast<float>(xmax_twips) / 20.0f;
    rect.ymin = static_cast<float>(ymin_twips) / 20.0f;
    rect.ymax = static_cast<float>(ymax_twips) / 20.0f;
    return rect;
}

void SWFStream::seek(size_t byte_offset) {
    if (byte_offset > size_) {
        throw std::out_of_range("SWFStream seek out of bounds");
    }
    byte_offset_ = byte_offset;
    bit_offset_ = 0;
}

size_t SWFStream::bytes_remaining() const {
    if (byte_offset_ >= size_) return 0;
    return size_ - byte_offset_;
}

} // namespace reflash
