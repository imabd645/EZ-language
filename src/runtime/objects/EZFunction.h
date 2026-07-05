#ifndef EZFUNCTION_H
#define EZFUNCTION_H

#include "runtime/Value.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <mutex>

struct EZFunction {
    std::string name;
    std::vector<std::string> params;
    std::vector<ExprPtr> defaultValues;
    std::vector<StmtPtr> body;
    std::shared_ptr<Environment> closure;
    std::shared_ptr<Environment> staticEnv;
    bool isVariadic;
    std::shared_ptr<struct BytecodeFunction> bytecode;

    EZFunction(const std::string& name,
               const std::vector<std::string>& params,
               const std::vector<ExprPtr>& defaultValues,
               const std::vector<StmtPtr>& body,
               std::shared_ptr<Environment> closure,
               bool variadic = false)
        : name(name), params(params), defaultValues(defaultValues), body(body),
          closure(closure), isVariadic(variadic) {}

    void traverse(const ValueVisitor& visit) const;
};

struct NativeFunction {
    std::string name;
    int arity;
    NativeFn function;
    NativeFunction(const std::string& name, int arity, NativeFn fn)
        : name(name), arity(arity), function(fn) {}
};

// ──────────────── Behavior flags ──────────────────────────────────────────────
struct BehaviorFlags {
    bool audited    : 1;
    bool snapshot   : 1;
    bool persistent : 1;
    bool validated  : 1;
    bool hasCached  : 1;
    bool any() const { return audited || snapshot || persistent || validated || hasCached; }
};

// Per-field validator (lives on EZClass)
struct FieldValidator {
    std::string field;
    std::string rule;    // "minlen","maxlen","min","max","email","pattern","notnull"
    Value       param;   // Value::NIL for rules without params
    std::string message;
};

// Audit entry (lives in EZInstance's auditLog)
struct AuditEntry {
    std::string field;
    Value       oldValue;
    Value       newValue;
    std::string via;       // calling task name
    long long   timestamp; // ms since epoch
};

// Cached method result (lives in EZInstance's cacheStore)
struct CachedResult {
    Value                            result;
    std::unordered_set<std::string>  deps;    // self fields read during computation
    bool                             dirty = true;
};


#endif // EZFUNCTION_H
