#include "reflash/filter_engine.hpp"
#include <cassert>
#include <iostream>
#include <cmath>

constexpr float EPSILON = 1e-4f;

bool approx_eq(float a, float b) {
    return std::abs(a - b) < EPSILON;
}

void test_unpack_filter_flags() {
    std::cout << "[Test] test_unpack_filter_flags..." << std::endl;

    uint8_t passes = 0;
    bool is_inner = false;
    bool is_knockout = false;
    bool is_hide_object = false;
    bool is_full = false;

    // Test flags: passes = 3 (0x03), inner = true (0x80), knockout = true (0x40)
    uint8_t flags = 0x03 | 0x80 | 0x40;
    reflash::unpack_filter_flags(flags, passes, is_inner, is_knockout, is_hide_object, is_full);

    assert(passes == 3);
    assert(is_inner == true);
    assert(is_knockout == true);
    assert(is_hide_object == false);
    assert(is_full == false);

    // Test flags: passes = 5 (0x05), hide_object = true (0x20), full = true (0x10)
    flags = 0x05 | 0x20 | 0x10;
    reflash::unpack_filter_flags(flags, passes, is_inner, is_knockout, is_hide_object, is_full);

    assert(passes == 5);
    assert(is_inner == false);
    assert(is_knockout == false);
    assert(is_hide_object == true);
    assert(is_full == true);

    std::cout << "  -> test_unpack_filter_flags PASSED" << std::endl;
}

void test_shadow_offset_math() {
    std::cout << "[Test] test_shadow_offset_math..." << std::endl;

    float dist = 10.0f;
    float angle_rad = 0.0f; // 0 radians: cos(0) = 1, sin(0) = 0
    float width = 100.0f;
    float height = 50.0f;

    reflash::Vec2 offset = reflash::calculate_shadow_offset(dist, angle_rad, width, height);
    // off_x = -cos(0)*10/100 = -0.1
    // off_y = -sin(0)*10/50 = 0.0
    assert(approx_eq(offset.x, -0.1f));
    assert(approx_eq(offset.y, 0.0f));

    // Angle = M_PI / 2 (90 deg): cos = 0, sin = 1
    float pi_2 = 1.57079632679f;
    offset = reflash::calculate_shadow_offset(dist, pi_2, width, height);
    // off_x = 0
    // off_y = -sin(pi/2)*10/50 = -0.2
    assert(approx_eq(offset.x, 0.0f));
    assert(approx_eq(offset.y, -0.2f));

    std::cout << "  -> test_shadow_offset_math PASSED" << std::endl;
}

void test_filter_structs() {
    std::cout << "[Test] test_filter_structs..." << std::endl;

    reflash::Filter glow;
    glow.type = reflash::FilterType::Glow;
    glow.blur_x = 4.0f;
    glow.blur_y = 4.0f;
    glow.strength = 2.0f;
    glow.color = reflash::Vec4{1.0f, 0.0f, 0.0f, 1.0f};

    assert(glow.type == reflash::FilterType::Glow);
    assert(approx_eq(glow.blur_x, 4.0f));
    assert(approx_eq(glow.strength, 2.0f));

    reflash::Filter shadow;
    shadow.type = reflash::FilterType::DropShadow;
    shadow.distance = 5.0f;
    shadow.angle = 0.785398f; // 45 degrees
    assert(shadow.type == reflash::FilterType::DropShadow);

    std::cout << "  -> test_filter_structs PASSED" << std::endl;
}

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "Running Flash Filter Engine Unit Tests" << std::endl;
    std::cout << "==========================================" << std::endl;

    test_unpack_filter_flags();
    test_shadow_offset_math();
    test_filter_structs();

    std::cout << "==========================================" << std::endl;
    std::cout << "All Flash Filter Unit Tests PASSED!" << std::endl;
    std::cout << "==========================================" << std::endl;
    return 0;
}
