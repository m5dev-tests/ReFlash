#ifndef REFLASH_AVM2_CONTEXT_HPP
#define REFLASH_AVM2_CONTEXT_HPP

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

// Ensure interpreter macros are defined
#ifndef AVMPLUS_INTERPRETER
#define AVMPLUS_INTERPRETER 1
#endif

#ifndef AVMPLUS_NO_JIT
#define AVMPLUS_NO_JIT 1
#endif

namespace reflash {

namespace avmplus {

// Forward-declarations / lightweight wrappers for AVM2 core components
class GC {
public:
    GC() : initialized_(true) {}
    ~GC() = default;
    bool is_initialized() const { return initialized_; }
private:
    bool initialized_{false};
};

class AvmCore {
public:
    explicit AvmCore(GC* gc) : gc_(gc), initialized_(true) {}
    ~AvmCore() = default;

    bool is_initialized() const { return initialized_ && gc_ != nullptr; }
private:
    GC* gc_{nullptr};
    bool initialized_{false};
};

} // namespace avmplus

struct ABCBlock {
    uint32_t flags{0};
    std::string name;
    std::vector<uint8_t> bytecode;
};

class AVM2Context {
public:
    AVM2Context();
    ~AVM2Context();

    bool init();
    void shutdown();

    bool load_abc(const uint8_t* abc_bytes, size_t abc_length, const char* entry_class = nullptr);
    bool execute_entrypoint(const std::string& class_name);

    std::string get_capabilities_version() const { return capabilities_version_; }
    void set_capabilities_version(const std::string& version) { capabilities_version_ = version; }

    const std::vector<ABCBlock>& loaded_blocks() const { return loaded_blocks_; }
    bool is_class_registered(const std::string& class_name) const;

private:
    std::unique_ptr<avmplus::GC> gc_;
    std::unique_ptr<avmplus::AvmCore> core_;
    std::string capabilities_version_{"WIN 11,9,100,0"};
    std::vector<ABCBlock> loaded_blocks_;
    std::unordered_map<std::string, size_t> registered_classes_;
    bool initialized_{false};
};

AVM2Context& get_global_avm2_context();

} // namespace reflash

extern "C" {

bool reflash_execute_abc(const uint8_t* abc_bytes, size_t abc_length, const char* entry_class);

}

#endif // REFLASH_AVM2_CONTEXT_HPP
