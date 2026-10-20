#include "reflash/core.hpp"
#include <iostream>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define REFLASH_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define REFLASH_EXPORT
#endif

extern "C" REFLASH_EXPORT int main(int argc, char** argv) {
    std::cout << "Initializing ReFlash - Open-Source WebAssembly Flash Runtime Engine" << std::endl;
    reflash::Engine engine;
    if (!engine.init(800, 600)) {
        std::cerr << "Failed to initialize ReFlash engine" << std::endl;
        return 1;
    }
    std::cout << "ReFlash engine initialized successfully (" << engine.width() << "x" << engine.height() << ")" << std::endl;
    return 0;
}
