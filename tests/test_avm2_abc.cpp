#include "reflash/avm2_context.hpp"
#include "reflash/swf_parser.hpp"
#include <cassert>
#include <iostream>
#include <vector>

void test_avm2_context_init_and_capabilities() {
    std::cout << "[Test] test_avm2_context_init_and_capabilities..." << std::endl;

    reflash::AVM2Context ctx;
    assert(ctx.init());
    assert(ctx.get_capabilities_version() == "WIN 11,9,100,0");

    std::cout << "  -> test_avm2_context_init_and_capabilities PASSED" << std::endl;
}

void test_abc_bytecode_ingestion() {
    std::cout << "[Test] test_abc_bytecode_ingestion..." << std::endl;

    std::vector<uint8_t> dummy_abc = {0x10, 0x00, 0x2E, 0x00, 0x00, 0x00};
    bool ok = reflash_execute_abc(dummy_abc.data(), dummy_abc.size(), "com.reflash.MainApp");
    assert(ok);

    auto& global_ctx = reflash::get_global_avm2_context();
    assert(global_ctx.is_class_registered("com.reflash.MainApp"));
    assert(!global_ctx.is_class_registered("com.reflash.NonExistentClass"));

    assert(global_ctx.loaded_blocks().size() >= 1);
    assert(global_ctx.loaded_blocks().back().name == "com.reflash.MainApp");

    std::cout << "  -> test_abc_bytecode_ingestion PASSED" << std::endl;
}

void test_as3_gating_for_doabc() {
    std::cout << "[Test] test_as3_gating_for_doabc..." << std::endl;

    // Construct a SWF buffer with Tag 72 (DoABC) but FileAttributes is_as3 = false
    std::vector<uint8_t> payload;

    // Tag 69: FileAttributes with AS3 bit = 0
    payload.push_back(0x44); payload.push_back(0x11);
    payload.push_back(0x00); // flags = 0 (AS2 / legacy mode)
    payload.push_back(0x00); payload.push_back(0x00); payload.push_back(0x00);

    // Tag 72: DoABC (TagCode 72 -> 0x1200)
    // TagHeader: (72 << 6) | 10 = 0x120A -> LE: 0x0A, 0x12
    payload.push_back(0x0A); payload.push_back(0x12);
    // Flags (0)
    payload.push_back(0x00); payload.push_back(0x00); payload.push_back(0x00); payload.push_back(0x00);
    // Name: "LegacyAS2Class\0"
    std::string name = "LegacyAS2Class";
    payload.insert(payload.end(), name.begin(), name.end());
    payload.push_back(0x00);
    // Dummy bytecode
    payload.push_back(0xAA); payload.push_back(0xBB);

    // End Tag
    payload.push_back(0x00); payload.push_back(0x00);

    uint32_t total_len = 8 + 12 + static_cast<uint32_t>(payload.size()); // 12 bytes RECT + FR + FC
    std::vector<uint8_t> swf = {
        'F', 'W', 'S', 9,
        static_cast<uint8_t>(total_len & 0xFF),
        static_cast<uint8_t>((total_len >> 8) & 0xFF),
        static_cast<uint8_t>((total_len >> 16) & 0xFF),
        static_cast<uint8_t>((total_len >> 24) & 0xFF),
        0x78, 0x00, 0x0F, 0xA0, 0x00, 0x0C, 0x80, 0x00, // RECT 0..100px
        0x00, 0x18, // 24 FPS
        0x01, 0x00  // 1 frame
    };
    swf.insert(swf.end(), payload.begin(), payload.end());

    reflash::SWFDocument doc;
    bool ok = doc.parse(swf.data(), swf.size());
    assert(ok);
    assert(!doc.is_as3());

    // Verify LegacyAS2Class was NOT registered because AS3 bit was false
    assert(!reflash::get_global_avm2_context().is_class_registered("LegacyAS2Class"));

    std::cout << "  -> test_as3_gating_for_doabc PASSED" << std::endl;
}

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "Running AVM2 ABC & Context Unit Tests" << std::endl;
    std::cout << "==========================================" << std::endl;

    test_avm2_context_init_and_capabilities();
    test_abc_bytecode_ingestion();
    test_as3_gating_for_doabc();

    std::cout << "==========================================" << std::endl;
    std::cout << "All AVM2 Unit Tests PASSED!" << std::endl;
    std::cout << "==========================================" << std::endl;
    return 0;
}
