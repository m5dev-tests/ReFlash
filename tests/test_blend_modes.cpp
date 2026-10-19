#include "reflash/blend_modes.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

constexpr float EPSILON = 1e-4f;

bool approx_equal(float a, float b) {
    return std::abs(a - b) < EPSILON;
}

bool color_approx_equal(const reflash::Color& c1, const reflash::Color& c2) {
    return approx_equal(c1.r, c2.r) &&
           approx_equal(c1.g, c2.g) &&
           approx_equal(c1.b, c2.b) &&
           approx_equal(c1.a, c2.a);
}

void test_add_blend() {
    // Src: (0.6, 0.4, 0.2, 1.0), Dst: (0.5, 0.7, 0.1, 1.0)
    // Add result (rgb): min(0.6+0.5, 1.0) = 1.0, min(0.4+0.7, 1.0) = 1.0, min(0.2+0.1, 1.0) = 0.3
    reflash::Color src{0.6f, 0.4f, 0.2f, 1.0f};
    reflash::Color dst{0.5f, 0.7f, 0.1f, 1.0f};

    reflash::Color res = reflash::blend_pixel(reflash::BlendMode::Add, src, dst);
    reflash::Color expected{1.0f, 1.0f, 0.3f, 1.0f};

    assert(color_approx_equal(res, expected));
    std::cout << "[PASS] Add Blend Mode test" << std::endl;
}

void test_multiply_blend() {
    // Src: (0.5, 0.8, 0.2, 1.0), Dst: (0.4, 0.5, 1.0, 1.0)
    // Multiply result (rgb): 0.5*0.4 = 0.2, 0.8*0.5 = 0.4, 0.2*1.0 = 0.2
    reflash::Color src{0.5f, 0.8f, 0.2f, 1.0f};
    reflash::Color dst{0.4f, 0.5f, 1.0f, 1.0f};

    reflash::Color res = reflash::blend_pixel(reflash::BlendMode::Multiply, src, dst);
    reflash::Color expected{0.2f, 0.4f, 0.2f, 1.0f};

    assert(color_approx_equal(res, expected));
    std::cout << "[PASS] Multiply Blend Mode test" << std::endl;
}

void test_screen_blend() {
    // Src: (0.5, 0.2, 0.0, 1.0), Dst: (0.5, 0.5, 0.5, 1.0)
    // Screen result (rgb): 1 - (1-0.5)*(1-0.5) = 0.75, 1 - (1-0.2)*(1-0.5) = 0.6, 1 - (1-0)*(1-0.5) = 0.5
    reflash::Color src{0.5f, 0.2f, 0.0f, 1.0f};
    reflash::Color dst{0.5f, 0.5f, 0.5f, 1.0f};

    reflash::Color res = reflash::blend_pixel(reflash::BlendMode::Screen, src, dst);
    reflash::Color expected{0.75f, 0.60f, 0.50f, 1.0f};

    assert(color_approx_equal(res, expected));
    std::cout << "[PASS] Screen Blend Mode test" << std::endl;
}

void test_difference_invert_blend() {
    // Src: (0.8, 0.2, 0.5, 1.0), Dst: (0.3, 0.6, 0.5, 1.0)
    // Difference result (rgb): |0.3-0.8| = 0.5, |0.6-0.2| = 0.4, |0.5-0.5| = 0.0
    reflash::Color src{0.8f, 0.2f, 0.5f, 1.0f};
    reflash::Color dst{0.3f, 0.6f, 0.5f, 1.0f};

    reflash::Color res = reflash::blend_pixel(reflash::BlendMode::Difference, src, dst);
    reflash::Color expected{0.5f, 0.4f, 0.0f, 1.0f};

    assert(color_approx_equal(res, expected));
    std::cout << "[PASS] Difference / Invert Blend Mode test" << std::endl;
}

void test_color_transform() {
    reflash::Color src{0.5f, 0.4f, 0.3f, 1.0f};
    reflash::ColorTransform ct{2.0f, 0.5f, 1.0f, 1.0f, 0.1f, -0.1f, 0.0f, 0.0f};

    reflash::Color res = reflash::apply_color_transform(src, ct);
    // Expected: r = clamp(0.5*2.0 + 0.1) = 1.0, g = clamp(0.4*0.5 - 0.1) = 0.1, b = 0.3, a = 1.0
    reflash::Color expected{1.0f, 0.1f, 0.3f, 1.0f};

    assert(color_approx_equal(res, expected));
    std::cout << "[PASS] ColorTransform test" << std::endl;
}

int main() {
    std::cout << "Running ReFlash CPU Blend Mode & ColorTransform Unit Tests..." << std::endl;
    test_add_blend();
    test_multiply_blend();
    test_screen_blend();
    test_difference_invert_blend();
    test_color_transform();
    std::cout << "All ReFlash blend mode unit tests PASSED successfully!" << std::endl;
    return 0;
}
