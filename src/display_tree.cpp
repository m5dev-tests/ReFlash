#include "reflash/display_tree.hpp"
#include "reflash/renderer.hpp"
#include "reflash/filter_engine.hpp"
#include "reflash/swf_parser.hpp"
#include <algorithm>

namespace reflash {

static Stage g_stage;

Stage& get_global_stage() {
    return g_stage;
}

void DisplayObject::update_world_matrix() {
    if (parent_) {
        world_matrix_ = local_matrix_.multiply(parent_->world_matrix());
    } else {
        world_matrix_ = local_matrix_;
    }
}

void DisplayObject::render() {
    if (!visible_) return;
    // Bind render state (BlendMode & ColorTransform)
    Renderer renderer;
    renderer.set_blend_mode(blend_mode_);
    renderer.set_color_transform(color_transform_);

    if (!filters_.empty()) {
        auto& engine = get_global_filter_engine();
        if (engine.is_initialized()) {
            engine.begin_object_pass();
            for (const auto& filter : filters_) {
                engine.apply_filter(filter);
            }
            engine.end_object_pass();
        }
    }
}

void DisplayObjectContainer::add_child(std::shared_ptr<DisplayObject> child) {
    if (!child) return;
    child->set_parent(this);
    children_.push_back(child);
    sort_children_by_depth();
}

void DisplayObjectContainer::add_child_at_depth(std::shared_ptr<DisplayObject> child, uint16_t depth) {
    if (!child) return;
    remove_child_at_depth(depth);
    child->set_depth(depth);
    add_child(child);
}

void DisplayObjectContainer::remove_child(std::shared_ptr<DisplayObject> child) {
    if (!child) return;
    auto it = std::remove(children_.begin(), children_.end(), child);
    if (it != children_.end()) {
        child->set_parent(nullptr);
        children_.erase(it, children_.end());
    }
}

void DisplayObjectContainer::remove_child_at_depth(uint16_t depth) {
    for (auto it = children_.begin(); it != children_.end(); ++it) {
        if ((*it)->depth() == depth) {
            (*it)->set_parent(nullptr);
            children_.erase(it);
            break;
        }
    }
}

std::shared_ptr<DisplayObject> DisplayObjectContainer::get_child_at_depth(uint16_t depth) const {
    for (const auto& child : children_) {
        if (child->depth() == depth) {
            return child;
        }
    }
    return nullptr;
}

void DisplayObjectContainer::sort_children_by_depth() {
    std::stable_sort(children_.begin(), children_.end(), [](const std::shared_ptr<DisplayObject>& a, const std::shared_ptr<DisplayObject>& b) {
        return a->depth() < b->depth();
    });
}

void DisplayObjectContainer::update_world_matrix() {
    DisplayObject::update_world_matrix();
    for (auto& child : children_) {
        if (child) {
            child->update_world_matrix();
        }
    }
}

void DisplayObjectContainer::render() {
    if (!visible_) return;
    DisplayObject::render();
    for (auto& child : children_) {
        if (child && child->visible()) {
            child->render();
        }
    }
}

void MovieClip::goto_and_stop(uint32_t frame) {
    current_frame_ = std::clamp(frame, 1U, total_frames_);
    is_playing_ = false;
}

void MovieClip::goto_and_play(uint32_t frame) {
    current_frame_ = std::clamp(frame, 1U, total_frames_);
    is_playing_ = true;
}

bool MovieClip::load_movie(const std::string& url) {
    std::vector<uint8_t> buffer;
    if (!load_movie_from_vfs(url, buffer)) {
        return false;
    }

    size_t file_size = buffer.size();
    // 1. Strict Load Lifecycle: Set initial bytesLoaded = 0 and bytesTotal = fileSize (do NOT return 0/0)
    bytes_loaded_ = 0;
    bytes_total_ = file_size;
    is_initialized_ = false;
    loading_in_progress_ = true;

    // Parse child movie document
    std::shared_ptr<SWFDocument> child_doc = std::make_shared<SWFDocument>();
    if (!child_doc->parse(buffer.data(), buffer.size())) {
        loading_in_progress_ = false;
        return false;
    }

    set_total_frames(child_doc->header().frame_count);

    // 2. Initialization Sequence: Only start ticking onEnterFrame when bytesLoaded == bytesTotal
    bytes_loaded_ = file_size;
    is_initialized_ = true;
    loading_in_progress_ = false;

    // Timeline and Frame 1 tags are properly registered so _root.mc_stg.play() advances timeline to Frame 2
    return true;
}

void MovieClip::advance_frame() {
    if (!is_playing_) return;
    if (!can_dispatch_enter_frame()) return;
    current_frame_++;
    if (current_frame_ > total_frames_) {
        current_frame_ = 1;
    }
}

} // namespace reflash
