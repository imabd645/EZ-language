#ifndef RUNTIME_CONTEXT_H
#define RUNTIME_CONTEXT_H

#include <string>
#include <vector>
#include <memory>

#include "Value.h"

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
    virtual std::shared_ptr<Environment> getCurrentEnv() const = 0;
};

#endif // RUNTIME_CONTEXT_H
