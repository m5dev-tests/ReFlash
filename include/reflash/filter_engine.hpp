#ifndef REFLASH_FILTER_ENGINE_HPP
#define REFLASH_FILTER_ENGINE_HPP

#include <cmath>
#include <cstdint>
#include <vector>
#include <memory>

namespace reflash {

struct Vec2 {
    float x{0.0f};
    float y{0.0f};
};

struct Vec4 {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
    float a{0.0f};
};

enum class FilterType {
    DropShadow = 0,
    Blur = 1,
    Glow = 2,
    Bevel = 3
};

struct Filter {
    FilterType type{FilterType::Blur};
    float blur_x{0.0f};
    float blur_y{0.0f};
    float distance{0.0f};
    float angle{0.0f}; // In radians
    float strength{1.0f};
    Vec4 color{0.0f, 0.0f, 0.0f, 1.0f};
    Vec4 highlight{0.0f, 0.0f, 0.0f, 0.0f}; // For Bevel
    uint8_t passes{1};
    bool is_inner{false};
    bool is_knockout{false};
    bool is_hide_object{false};
    bool is_full{false};
};

// Bitmask unpacking helper as described in spec
inline void unpack_filter_flags(uint8_t flags, uint8_t& passes, bool& is_inner, bool& is_knockout, bool& is_hide_object, bool& is_full) {
    passes = flags & 0x0F;
    if (passes == 0) passes = 1;
    is_inner = (flags & 0x80) != 0;
    is_knockout = (flags & 0x40) != 0;
    is_hide_object = (flags & 0x20) != 0;
    is_full = (flags & 0x10) != 0;
}

// Shadow offset calculation helper: (-cos(angle) * dist / width, -sin(angle) * dist / height)
inline Vec2 calculate_shadow_offset(float distance, float angle_rad, float width, float height) {
    if (width <= 0.0f || height <= 0.0f) {
        return Vec2{0.0f, 0.0f};
    }
    float off_x = -std::cos(angle_rad) * distance / width;
    float off_y = -std::sin(angle_rad) * distance / height;
    return Vec2{off_x, off_y};
}

class FilterEngine {
public:
    FilterEngine() = default;
    ~FilterEngine();

    bool init(int width, int height);
    void shutdown();
    void resize(int width, int height);

    void begin_object_pass();
    void apply_filter(const Filter& filter);
    void end_object_pass();

    bool is_initialized() const { return initialized_; }

private:
    bool initialized_{false};
    int width_{800};
    int height_{600};

    unsigned int fbo_a_{0}, tex_a_{0};
    unsigned int fbo_b_{0}, tex_b_{0};
    unsigned int fbo_c_{0}, tex_c_{0};

    unsigned int blur_program_{0};
    unsigned int composite_program_{0};
    unsigned int quad_vao_{0}, quad_vbo_{0};

    void setup_framebuffers(int width, int height);
    void cleanup_framebuffers();
    void init_shaders();
};

FilterEngine& get_global_filter_engine();

} // namespace reflash

#endif // REFLASH_FILTER_ENGINE_HPP
