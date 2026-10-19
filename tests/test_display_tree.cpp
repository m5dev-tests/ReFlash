#include "reflash/display_tree.hpp"
#include <cassert>
#include <iostream>
#include <cmath>

constexpr float EPSILON = 1e-4f;

bool approx_eq(float a, float b) {
    return std::abs(a - b) < EPSILON;
}

void test_matrix_concatenation() {
    std::cout << "[Test] test_matrix_concatenation..." << std::endl;

    // Parent container translated by (10, 20), scaled by 2x
    auto parent = std::make_shared<reflash::DisplayObjectContainer>();
    reflash::Matrix parent_mat;
    parent_mat.a = 2.0f;
    parent_mat.d = 2.0f;
    parent_mat.tx = 10.0f;
    parent_mat.ty = 20.0f;
    parent->set_local_matrix(parent_mat);

    // Child display object translated by (5, 5) relative to parent
    auto child = std::make_shared<reflash::DisplayObject>();
    reflash::Matrix child_mat;
    child_mat.tx = 5.0f;
    child_mat.ty = 5.0f;
    child->set_local_matrix(child_mat);

    parent->add_child(child);
    parent->update_world_matrix();

    // World matrix of parent: tx=10, ty=20
    assert(approx_eq(parent->world_matrix().tx, 10.0f));
    assert(approx_eq(parent->world_matrix().ty, 20.0f));

    // World matrix of child: tx = 5*2 + 10 = 20, ty = 5*2 + 20 = 30
    assert(approx_eq(child->world_matrix().tx, 20.0f));
    assert(approx_eq(child->world_matrix().ty, 30.0f));

    std::cout << "  -> test_matrix_concatenation PASSED" << std::endl;
}

void test_depth_sorting() {
    std::cout << "[Test] test_depth_sorting..." << std::endl;

    auto container = std::make_shared<reflash::DisplayObjectContainer>();

    auto obj_depth5 = std::make_shared<reflash::DisplayObject>();
    obj_depth5->set_name("Obj5");

    auto obj_depth2 = std::make_shared<reflash::DisplayObject>();
    obj_depth2->set_name("Obj2");

    auto obj_depth10 = std::make_shared<reflash::DisplayObject>();
    obj_depth10->set_name("Obj10");

    // Add children out of depth order
    container->add_child_at_depth(obj_depth5, 5);
    container->add_child_at_depth(obj_depth10, 10);
    container->add_child_at_depth(obj_depth2, 2);

    const auto& children = container->children();
    assert(children.size() == 3);
    assert(children[0]->depth() == 2);
    assert(children[1]->depth() == 5);
    assert(children[2]->depth() == 10);

    assert(children[0]->name() == "Obj2");
    assert(children[1]->name() == "Obj5");
    assert(children[2]->name() == "Obj10");

    std::cout << "  -> test_depth_sorting PASSED" << std::endl;
}

void test_movieclip_playback() {
    std::cout << "[Test] test_movieclip_playback..." << std::endl;

    reflash::MovieClip mc;
    mc.set_total_frames(5);
    assert(mc.current_frame() == 1);
    assert(mc.is_playing());

    mc.advance_frame();
    assert(mc.current_frame() == 2);

    mc.goto_and_stop(4);
    assert(mc.current_frame() == 4);
    assert(!mc.is_playing());

    mc.advance_frame(); // Should not advance since stopped
    assert(mc.current_frame() == 4);

    mc.play();
    mc.advance_frame();
    assert(mc.current_frame() == 5);

    mc.advance_frame(); // Should wrap around to 1
    assert(mc.current_frame() == 1);

    std::cout << "  -> test_movieclip_playback PASSED" << std::endl;
}

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "Running Display Tree Unit Tests" << std::endl;
    std::cout << "==========================================" << std::endl;

    test_matrix_concatenation();
    test_depth_sorting();
    test_movieclip_playback();

    std::cout << "==========================================" << std::endl;
    std::cout << "All Display Tree Unit Tests PASSED!" << std::endl;
    std::cout << "==========================================" << std::endl;
    return 0;
}
