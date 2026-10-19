#ifndef REFLASH_BLEND_MODES_HPP
#define REFLASH_BLEND_MODES_HPP

#include <cstdint>
#include <algorithm>
#include <cmath>

namespace reflash {

enum class BlendMode : int {
    Normal = 0,
    Layer = 1,
    Multiply = 2,
    Screen = 3,
    Lighten = 4,
    Darken = 5,
    Difference = 6,
    Add = 7,
    Subtract = 8,
    Invert = 9,
    Alpha = 10,
    Erase = 11,
    Overlay = 12,
    HardLight = 13
};

struct ColorTransform {
    float rMult{1.0f};
    float gMult{1.0f};
    float bMult{1.0f};
    float aMult{1.0f};

    float rAdd{0.0f};
    float gAdd{0.0f};
    float bAdd{0.0f};
    float aAdd{0.0f};
};

struct Color {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
    float a{0.0f};

    static Color from_uint8(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        return Color{r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
    }

    void clamp() {
        r = std::clamp(r, 0.0f, 1.0f);
        g = std::clamp(g, 0.0f, 1.0f);
        b = std::clamp(b, 0.0f, 1.0f);
        a = std::clamp(a, 0.0f, 1.0f);
    }
};

Color apply_color_transform(const Color& src, const ColorTransform& ct);
Color blend_pixel(BlendMode mode, const Color& src, const Color& dst);

} // namespace reflash

#endif // REFLASH_BLEND_MODES_HPP
