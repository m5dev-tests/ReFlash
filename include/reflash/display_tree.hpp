#ifndef REFLASH_DISPLAY_TREE_HPP
#define REFLASH_DISPLAY_TREE_HPP

#include "reflash/blend_modes.hpp"
#include "reflash/filter_engine.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace reflash {

struct Matrix {
    float a{1.0f}; // scale_x
    float b{0.0f}; // rotate_skew0
    float c{0.0f}; // rotate_skew1
    float d{1.0f}; // scale_y
    float tx{0.0f}; // translate_x
    float ty{0.0f}; // translate_y

    Matrix multiply(const Matrix& parent) const {
        Matrix res;
        res.a = a * parent.a + b * parent.c;
        res.b = a * parent.b + b * parent.d;
        res.c = c * parent.a + d * parent.c;
        res.d = c * parent.b + d * parent.d;
        res.tx = tx * parent.a + ty * parent.c + parent.tx;
        res.ty = tx * parent.b + ty * parent.d + parent.ty;
        return res;
    }
};

class DisplayObjectContainer;

class DisplayObject : public std::enable_shared_from_this<DisplayObject> {
public:
    DisplayObject() = default;
    virtual ~DisplayObject() = default;

    uint16_t character_id() const { return character_id_; }
    void set_character_id(uint16_t id) { character_id_ = id; }

    uint16_t depth() const { return depth_; }
    void set_depth(uint16_t depth) { depth_ = depth; }

    const std::string& name() const { return name_; }
    void set_name(const std::string& name) { name_ = name; }

    bool visible() const { return visible_; }
    void set_visible(bool visible) { visible_ = visible; }

    const Matrix& local_matrix() const { return local_matrix_; }
    void set_local_matrix(const Matrix& matrix) { local_matrix_ = matrix; }

    const Matrix& world_matrix() const { return world_matrix_; }

    const ColorTransform& color_transform() const { return color_transform_; }
    void set_color_transform(const ColorTransform& ct) { color_transform_ = ct; }

    BlendMode blend_mode() const { return blend_mode_; }
    void set_blend_mode(BlendMode mode) { blend_mode_ = mode; }

    const std::vector<Filter>& filters() const { return filters_; }
    void set_filters(const std::vector<Filter>& filters) { filters_ = filters; }
    void add_filter(const Filter& filter) { filters_.push_back(filter); }
    void clear_filters() { filters_.clear(); }

    DisplayObjectContainer* parent() const { return parent_; }
    void set_parent(DisplayObjectContainer* parent) { parent_ = parent; }

    virtual void update_world_matrix();
    virtual void render();

protected:
    uint16_t character_id_{0};
    uint16_t depth_{0};
    std::string name_;
    bool visible_{true};

    Matrix local_matrix_;
    Matrix world_matrix_;
    ColorTransform color_transform_;
    BlendMode blend_mode_{BlendMode::Normal};
    std::vector<Filter> filters_;

    DisplayObjectContainer* parent_{nullptr};
};

class DisplayObjectContainer : public DisplayObject {
public:
    DisplayObjectContainer() = default;
    ~DisplayObjectContainer() override = default;

    void add_child(std::shared_ptr<DisplayObject> child);
    void add_child_at_depth(std::shared_ptr<DisplayObject> child, uint16_t depth);
    void remove_child(std::shared_ptr<DisplayObject> child);
    void remove_child_at_depth(uint16_t depth);
    std::shared_ptr<DisplayObject> get_child_at_depth(uint16_t depth) const;

    const std::vector<std::shared_ptr<DisplayObject>>& children() const { return children_; }

    void sort_children_by_depth();
    void update_world_matrix() override;
    void render() override;

protected:
    std::vector<std::shared_ptr<DisplayObject>> children_;
};

class MovieClip : public DisplayObjectContainer {
public:
    MovieClip() = default;
    ~MovieClip() override = default;

    uint32_t current_frame() const { return current_frame_; }
    uint32_t total_frames() const { return total_frames_; }
    void set_total_frames(uint32_t frames) { total_frames_ = frames; }

    bool is_playing() const { return is_playing_; }
    void play() { is_playing_ = true; }
    void stop() { is_playing_ = false; }
    void goto_and_stop(uint32_t frame);
    void goto_and_play(uint32_t frame);

    size_t bytes_loaded() const { return bytes_loaded_; }
    size_t bytes_total() const { return bytes_total_; }
    void set_bytes_loaded(size_t loaded) { bytes_loaded_ = loaded; }
    void set_bytes_total(size_t total) { bytes_total_ = total; }

    bool is_initialized() const { return is_initialized_; }
    void set_initialized(bool init) { is_initialized_ = init; }

    bool is_loader_clip() const { return is_loader_clip_; }
    void set_is_loader_clip(bool loader) { is_loader_clip_ = loader; }

    bool can_dispatch_enter_frame() const {
        if (loading_in_progress_) return false;
        if (is_loader_clip_ && (!is_initialized_ || bytes_loaded_ < bytes_total_)) {
            return false;
        }
        return true;
    }

    bool load_movie(const std::string& url);
    void advance_frame();

private:
    uint32_t current_frame_{1};
    uint32_t total_frames_{1};
    bool is_playing_{true};

    size_t bytes_loaded_{100};
    size_t bytes_total_{100};
    bool is_initialized_{true};
    bool is_loader_clip_{false};
    bool loading_in_progress_{false};
};

class Stage : public DisplayObjectContainer {
public:
    Stage() = default;
    ~Stage() override = default;

    void set_stage_size(float width, float height) {
        width_ = width;
        height_ = height;
    }

    float stage_width() const { return width_; }
    float stage_height() const { return height_; }

private:
    float width_{800.0f};
    float height_{600.0f};
};

Stage& get_global_stage();

} // namespace reflash

#endif // REFLASH_DISPLAY_TREE_HPP
