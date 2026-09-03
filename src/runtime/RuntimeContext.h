#ifndef RUNTIME_CONTEXT_H
#define RUNTIME_CONTEXT_H

#include <string>
#include <vector>
#include <memory>

#include "runtime/Value.h"

// Forward declarations
class Environment;

class RuntimeContext {
public:
    virtual ~RuntimeContext() = default;

    // Error reporting
    virtual void runtimeError(const std::string& message, int line = 0, const std::string& filename = "") = 0;
    virtual void throwException(const std::string& className, const std::string& message, int line = 0, const std::string& filename = "") = 0;
    virtual void printStackTrace() const = 0;

    // Environment access
    virtual std::shared_ptr<Environment> getGlobalEnv() = 0;
    virtual void defineGlobal(const std::string& name, const Value& value) = 0;

    // Dynamic execution (e.g. for eval)
    virtual Value eval(const std::string& code, const std::string& filename = "<eval>") = 0;
    virtual Value callFunction(const Value& callee, const std::vector<Value>& args, int line = 0, const std::string& filename = "native") = 0;
    
    // Utilities
    virtual std::string stringify(const Value& val, int line = 0, const std::string& filename = "") = 0;

    // Drop references held by stack slots above the current top.
    //
    // Popping only moves the stack pointer, so the vacated slots keep holding
    // whatever Value they last had. Those references are invisible to the
    // program but keep objects alive until the slot is overwritten -- a file
    // opened in a top-level loop stayed open after its variable was cleared,
    // because the last instance was still sitting in a dead slot. Called before
    // an explicit gc_collect() so a requested collection sees the true
    // reachable set. Default is a no-op for contexts with no value stack.
    virtual void releaseStaleStackSlots() {}
    virtual std::shared_ptr<Environment> getCurrentEnv() const = 0;

    // VM limits & introspection
    virtual size_t getMaxRecursionDepth() const { return 4096; }
    virtual void setMaxRecursionDepth(size_t depth) {}
    virtual size_t getCallDepth() const { return 0; }
    virtual uint64_t getInstructionCount() const { return 0; }
    virtual void resetInstructionCount() {}
    virtual uint64_t getMaxInstructions() const { return 0; }
    virtual void setMaxInstructions(uint64_t max) {}
    virtual std::vector<Value> getStackTraceFrames() const { return {}; }
};

#endif // RUNTIME_CONTEXT_H
