#include "reflash/events.hpp"
#include "reflash/display_tree.hpp"
#include "reflash/avm2_context.hpp"
#include <iostream>

namespace reflash {

static InputManager g_input_manager;

InputManager& get_global_input_manager() {
    return g_input_manager;
}

std::string event_type_to_string(EventType type) {
    switch (type) {
        case EventType::MouseDown: return "MOUSE_DOWN";
        case EventType::MouseUp:   return "MOUSE_UP";
        case EventType::MouseMove: return "MOUSE_MOVE";
        case EventType::RollOver:  return "ROLL_OVER";
        case EventType::RollOut:   return "ROLL_OUT";
        case EventType::Click:     return "CLICK";
        case EventType::KeyDown:   return "KEY_DOWN";
        case EventType::KeyUp:     return "KEY_UP";
    }
    return "UNKNOWN";
}

void EventDispatcher::add_event_listener(EventType type, EventCallback callback) {
    if (callback) {
        listeners_[type].push_back(callback);
    }
}

void EventDispatcher::remove_event_listeners(EventType type) {
    listeners_.erase(type);
}

void EventDispatcher::dispatch_event(const Event& event) {
    auto it = listeners_.find(event.type);
    if (it != listeners_.end()) {
        for (const auto& cb : it->second) {
            if (cb) {
                cb(event);
            }
        }
    }
}

void InputManager::reset() {
    hovered_object_.reset();
    pressed_object_.reset();
    mouse_x_ = 0.0f;
    mouse_y_ = 0.0f;
}

void InputManager::handle_mouse_move(float stage_x, float stage_y) {
    mouse_x_ = stage_x;
    mouse_y_ = stage_y;

    auto stage_ptr = get_global_stage_ptr();
    stage_ptr->update_world_matrix();

    std::shared_ptr<DisplayObject> target = stage_ptr->hit_test(stage_x, stage_y);

    if (hovered_object_ != target) {
        if (hovered_object_) {
            Event roll_out_evt;
            roll_out_evt.type = EventType::RollOut;
            roll_out_evt.type_name = event_type_to_string(EventType::RollOut);
            roll_out_evt.stage_x = stage_x;
            roll_out_evt.stage_y = stage_y;
            roll_out_evt.target = hovered_object_;
            hovered_object_->dispatch_event(roll_out_evt);

            auto btn = std::dynamic_pointer_cast<SimpleButton>(hovered_object_);
            if (btn && btn->button_state() != ButtonState::Down) {
                btn->set_button_state(ButtonState::Up);
            }
        }

        hovered_object_ = target;

        if (hovered_object_) {
            Event roll_over_evt;
            roll_over_evt.type = EventType::RollOver;
            roll_over_evt.type_name = event_type_to_string(EventType::RollOver);
            roll_over_evt.stage_x = stage_x;
            roll_over_evt.stage_y = stage_y;
            roll_over_evt.target = hovered_object_;
            hovered_object_->dispatch_event(roll_over_evt);

            auto btn = std::dynamic_pointer_cast<SimpleButton>(hovered_object_);
            if (btn && btn->button_state() != ButtonState::Down) {
                btn->set_button_state(ButtonState::Over);
            }
        }
    }

    Event move_evt;
    move_evt.type = EventType::MouseMove;
    move_evt.type_name = event_type_to_string(EventType::MouseMove);
    move_evt.stage_x = stage_x;
    move_evt.stage_y = stage_y;
    move_evt.target = hovered_object_ ? hovered_object_ : stage_ptr;

    if (hovered_object_) {
        hovered_object_->dispatch_event(move_evt);
    } else {
        stage_ptr->dispatch_event(move_evt);
    }
}

void InputManager::handle_mouse_down(int button, float stage_x, float stage_y) {
    mouse_x_ = stage_x;
    mouse_y_ = stage_y;

    auto stage_ptr = get_global_stage_ptr();
    stage_ptr->update_world_matrix();

    std::shared_ptr<DisplayObject> target = stage_ptr->hit_test(stage_x, stage_y);
    pressed_object_ = target;

    if (pressed_object_) {
        auto btn = std::dynamic_pointer_cast<SimpleButton>(pressed_object_);
        if (btn) {
            btn->set_button_state(ButtonState::Down);
        }

        Event down_evt;
        down_evt.type = EventType::MouseDown;
        down_evt.type_name = event_type_to_string(EventType::MouseDown);
        down_evt.stage_x = stage_x;
        down_evt.stage_y = stage_y;
        down_evt.button = button;
        down_evt.target = pressed_object_;
        pressed_object_->dispatch_event(down_evt);
    } else {
        Event down_evt;
        down_evt.type = EventType::MouseDown;
        down_evt.type_name = event_type_to_string(EventType::MouseDown);
        down_evt.stage_x = stage_x;
        down_evt.stage_y = stage_y;
        down_evt.button = button;
        down_evt.target = stage_ptr;
        stage_ptr->dispatch_event(down_evt);
    }
}

void InputManager::handle_mouse_up(int button, float stage_x, float stage_y) {
    mouse_x_ = stage_x;
    mouse_y_ = stage_y;

    auto stage_ptr = get_global_stage_ptr();
    stage_ptr->update_world_matrix();

    std::shared_ptr<DisplayObject> target = stage_ptr->hit_test(stage_x, stage_y);

    if (pressed_object_) {
        auto btn = std::dynamic_pointer_cast<SimpleButton>(pressed_object_);
        if (btn) {
            if (target == pressed_object_) {
                btn->set_button_state(ButtonState::Over);
            } else {
                btn->set_button_state(ButtonState::Up);
            }
        }

        Event up_evt;
        up_evt.type = EventType::MouseUp;
        up_evt.type_name = event_type_to_string(EventType::MouseUp);
        up_evt.stage_x = stage_x;
        up_evt.stage_y = stage_y;
        up_evt.button = button;
        up_evt.target = pressed_object_;
        pressed_object_->dispatch_event(up_evt);

        if (target == pressed_object_) {
            Event click_evt;
            click_evt.type = EventType::Click;
            click_evt.type_name = event_type_to_string(EventType::Click);
            click_evt.stage_x = stage_x;
            click_evt.stage_y = stage_y;
            click_evt.button = button;
            click_evt.target = pressed_object_;
            pressed_object_->dispatch_event(click_evt);
        }

        pressed_object_.reset();
    } else {
        Event up_evt;
        up_evt.type = EventType::MouseUp;
        up_evt.type_name = event_type_to_string(EventType::MouseUp);
        up_evt.stage_x = stage_x;
        up_evt.stage_y = stage_y;
        up_evt.button = button;
        up_evt.target = target ? target : stage_ptr;

        if (target) {
            target->dispatch_event(up_evt);
        } else {
            stage_ptr->dispatch_event(up_evt);
        }
    }
}

void InputManager::handle_key_down(int keycode) {
    auto stage_ptr = get_global_stage_ptr();
    Event key_evt;
    key_evt.type = EventType::KeyDown;
    key_evt.type_name = event_type_to_string(EventType::KeyDown);
    key_evt.keycode = keycode;
    key_evt.target = hovered_object_ ? hovered_object_ : stage_ptr;

    if (hovered_object_) {
        hovered_object_->dispatch_event(key_evt);
    } else {
        stage_ptr->dispatch_event(key_evt);
    }
}

void InputManager::handle_key_up(int keycode) {
    auto stage_ptr = get_global_stage_ptr();
    Event key_evt;
    key_evt.type = EventType::KeyUp;
    key_evt.type_name = event_type_to_string(EventType::KeyUp);
    key_evt.keycode = keycode;
    key_evt.target = hovered_object_ ? hovered_object_ : stage_ptr;

    if (hovered_object_) {
        hovered_object_->dispatch_event(key_evt);
    } else {
        stage_ptr->dispatch_event(key_evt);
    }
}

} // namespace reflash
