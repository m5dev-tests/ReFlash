#ifndef REFLASH_EVENTS_HPP
#define REFLASH_EVENTS_HPP

#include <string>
#include <functional>
#include <vector>
#include <unordered_map>
#include <memory>

namespace reflash {

class DisplayObject;

enum class EventType {
    MouseDown,
    MouseUp,
    MouseMove,
    RollOver,
    RollOut,
    Click,
    KeyDown,
    KeyUp
};

std::string event_type_to_string(EventType type);

struct Event {
    EventType type{EventType::MouseMove};
    std::string type_name;
    float stage_x{0.0f};
    float stage_y{0.0f};
    int button{0}; // 0 = Left, 1 = Middle, 2 = Right
    int keycode{0};
    std::shared_ptr<DisplayObject> target;
};

using EventCallback = std::function<void(const Event&)>;

class EventDispatcher {
public:
    EventDispatcher() = default;
    virtual ~EventDispatcher() = default;

    void add_event_listener(EventType type, EventCallback callback);
    void remove_event_listeners(EventType type);
    void dispatch_event(const Event& event);

private:
    std::unordered_map<EventType, std::vector<EventCallback>> listeners_;
};

class InputManager {
public:
    InputManager() = default;
    ~InputManager() = default;

    void handle_mouse_move(float stage_x, float stage_y);
    void handle_mouse_down(int button, float stage_x, float stage_y);
    void handle_mouse_up(int button, float stage_x, float stage_y);
    void handle_key_down(int keycode);
    void handle_key_up(int keycode);

    std::shared_ptr<DisplayObject> hovered_object() const { return hovered_object_; }
    std::shared_ptr<DisplayObject> pressed_object() const { return pressed_object_; }
    float mouse_x() const { return mouse_x_; }
    float mouse_y() const { return mouse_y_; }

    void reset();

private:
    float mouse_x_{0.0f};
    float mouse_y_{0.0f};
    std::shared_ptr<DisplayObject> hovered_object_;
    std::shared_ptr<DisplayObject> pressed_object_;
};

InputManager& get_global_input_manager();

} // namespace reflash

#endif // REFLASH_EVENTS_HPP
