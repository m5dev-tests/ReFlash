#include "reflash/renderer.hpp"
#include "reflash/core.hpp"
#include <iostream>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <GLES3/gl3.h>
#else
// For native compilation/testing without OpenGL linking
#ifndef GL_FLOAT
typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef uintptr_t GLsizeiptr;
typedef float GLfloat;
typedef char GLchar;
#define GL_FLOAT 0x1406
#define GL_FALSE 0
#define GL_TRIANGLES 0x0004
#define GL_COLOR_BUFFER_BIT 0x00004000
#endif
#endif

namespace reflash {

static Renderer g_renderer;
static Engine g_engine;

bool Renderer::init() {
#ifdef __EMSCRIPTEN__
    // WebGL2 GL setup
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
#endif
    initialized_ = true;
    return true;
}

void Renderer::shutdown() {
    initialized_ = false;
}

void Renderer::begin_frame(int width, int height) {
#ifdef __EMSCRIPTEN__
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);
#endif
}

void Renderer::end_frame() {
    // Present frame
}

void Renderer::set_blend_mode(BlendMode mode) {
    current_blend_mode_ = mode;
}

void Renderer::set_color_transform(const ColorTransform& ct) {
    current_ct_ = ct;
}

void Renderer::draw_rect(float x, float y, float w, float h, const Color& color) {
    // Drawing implementation
}

bool Engine::init(int width, int height) {
    width_ = width;
    height_ = height;
    if (!g_renderer.init()) {
        return false;
    }
    initialized_ = true;
    return true;
}

void Engine::update(float deltaTime) {
    (void)deltaTime;
}

void Engine::render() {
    if (!initialized_) return;
    g_renderer.begin_frame(width_, height_);
    g_renderer.end_frame();
}

void Engine::resize(int width, int height) {
    width_ = width;
    height_ = height;
}

} // namespace reflash

// Exported C functions for Wasm runtime interaction
extern "C" {

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
int init_engine(int width, int height) {
    return reflash::g_engine.init(width, height) ? 1 : 0;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void load_frame() {
    reflash::g_engine.update(1.0f / 60.0f);
    reflash::g_engine.render();
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void set_blend_mode(int mode) {
    reflash::g_renderer.set_blend_mode(static_cast<reflash::BlendMode>(mode));
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void set_color_transform(float rM, float gM, float bM, float aM, float rA, float gA, float bA, float aA) {
    reflash::ColorTransform ct{rM, gM, bM, aM, rA, gA, bA, aA};
    reflash::g_renderer.set_color_transform(ct);
}

}
