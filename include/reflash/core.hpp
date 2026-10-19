#ifndef REFLASH_CORE_HPP
#define REFLASH_CORE_HPP

#include <string>
#include <cstdint>

namespace reflash {

class Engine {
public:
    Engine() = default;
    ~Engine() = default;

    bool init(int width, int height);
    void update(float deltaTime);
    void render();
    void resize(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }
    bool is_initialized() const { return initialized_; }

private:
    int width_{800};
    int height_{600};
    bool initialized_{false};
};

} // namespace reflash

#endif // REFLASH_CORE_HPP
