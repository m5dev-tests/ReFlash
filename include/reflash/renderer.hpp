#ifndef REFLASH_RENDERER_HPP
#define REFLASH_RENDERER_HPP

#include "reflash/blend_modes.hpp"

namespace reflash {

class Renderer {
public:
    Renderer() = default;
    ~Renderer() = default;

    bool init();
    void shutdown();

    void begin_frame(int width, int height);
    void end_frame();

    void set_blend_mode(BlendMode mode);
    void set_color_transform(const ColorTransform& ct);

    void draw_rect(float x, float y, float w, float h, const Color& color);

private:
    BlendMode current_blend_mode_{BlendMode::Normal};
    ColorTransform current_ct_{};
    bool initialized_{false};
    unsigned int program_{0};
    unsigned int vao_{0};
    unsigned int vbo_{0};
};

} // namespace reflash

#endif // REFLASH_RENDERER_HPP
