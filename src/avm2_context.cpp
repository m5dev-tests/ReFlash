#include "reflash/avm2_context.hpp"
#include <iostream>
#include <cstring>

namespace reflash {

static AVM2Context g_avm2_context;

AVM2Context::AVM2Context() = default;

AVM2Context::~AVM2Context() {
    shutdown();
}

bool AVM2Context::init() {
    if (initialized_) return true;

    gc_ = std::make_unique<avmplus::GC>();
    core_ = std::make_unique<avmplus::AvmCore>(gc_.get());

    // Flash Player version spoofing to satisfy modern capability checks
    capabilities_version_ = "WIN 11,9,100,0";

    initialized_ = core_->is_initialized();
    return initialized_;
}

void AVM2Context::shutdown() {
    loaded_blocks_.clear();
    registered_classes_.clear();
    core_.reset();
    gc_.reset();
    initialized_ = false;
}

bool AVM2Context::load_abc(const uint8_t* abc_bytes, size_t abc_length, const char* entry_class) {
    if (!initialized_) {
        if (!init()) return false;
    }

    if (!abc_bytes || abc_length == 0) return false;

    ABCBlock block;
    block.flags = 0;
    block.name = entry_class ? entry_class : "UnnamedABC";
    block.bytecode.assign(abc_bytes, abc_bytes + abc_length);

    loaded_blocks_.push_back(block);

    if (entry_class && std::strlen(entry_class) > 0) {
        registered_classes_[entry_class] = loaded_blocks_.size() - 1;
    }

    return true;
}

bool AVM2Context::execute_entrypoint(const std::string& class_name) {
    if (!initialized_) return false;
    return is_class_registered(class_name);
}

bool AVM2Context::is_class_registered(const std::string& class_name) const {
    return registered_classes_.find(class_name) != registered_classes_.end();
}

AVM2Context& get_global_avm2_context() {
    return g_avm2_context;
}

} // namespace reflash

extern "C" {

bool reflash_execute_abc(const uint8_t* abc_bytes, size_t abc_length, const char* entry_class) {
    return reflash::get_global_avm2_context().load_abc(abc_bytes, abc_length, entry_class);
}

}
