#include "reflash/blend_modes.hpp"

namespace reflash {

Color apply_color_transform(const Color& src, const ColorTransform& ct) {
    Color res;
    res.r = src.r * ct.rMult + ct.rAdd;
    res.g = src.g * ct.gMult + ct.gAdd;
    res.b = src.b * ct.bMult + ct.bAdd;
    res.a = src.a * ct.aMult + ct.aAdd;
    res.clamp();
    return res;
}

Color blend_pixel(BlendMode mode, const Color& src, const Color& dst) {
    Color res;
    // Perform RGB blend based on specified mode
    switch (mode) {
        case BlendMode::Add:
            res.r = std::min(src.r + dst.r, 1.0f);
            res.g = std::min(src.g + dst.g, 1.0f);
            res.b = std::min(src.b + dst.b, 1.0f);
            break;
        case BlendMode::Multiply:
            res.r = src.r * dst.r;
            res.g = src.g * dst.g;
            res.b = src.b * dst.b;
            break;
        case BlendMode::Screen:
            res.r = 1.0f - (1.0f - src.r) * (1.0f - dst.r);
            res.g = 1.0f - (1.0f - src.g) * (1.0f - dst.g);
            res.b = 1.0f - (1.0f - src.b) * (1.0f - dst.b);
            break;
        case BlendMode::Invert:
        case BlendMode::Difference:
            res.r = std::abs(dst.r - src.r);
            res.g = std::abs(dst.g - src.g);
            res.b = std::abs(dst.b - src.b);
            break;
        case BlendMode::Normal:
        default:
            // Standard premultiplied / alpha blend
            res.r = src.r * src.a + dst.r * (1.0f - src.a);
            res.g = src.g * src.a + dst.g * (1.0f - src.a);
            res.b = src.b * src.a + dst.b * (1.0f - src.a);
            res.a = src.a + dst.a * (1.0f - src.a);
            return res;
    }

    // For non-normal blend modes, blend result with destination using source alpha
    res.r = res.r * src.a + dst.r * (1.0f - src.a);
    res.g = res.g * src.a + dst.g * (1.0f - src.a);
    res.b = res.b * src.a + dst.b * (1.0f - src.a);
    res.a = src.a + dst.a * (1.0f - src.a);

    res.clamp();
    return res;
}

} // namespace reflash
