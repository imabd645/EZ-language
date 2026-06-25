// Runtime.cpp
// Miscellaneous out-of-line definitions that depend on fully-defined types
// (Environment, EZFunction, etc.). Formerly split across GC.cpp.

#include "Environment.h"
#include "Value.h"
#include "CycleCollector.h"

// ── Thread-local string interning pool ───────────────────────────────────────
thread_local std::unordered_map<std::string, std::weak_ptr<std::string>> globalStringPool;

// ── EZFunction::traverse() ───────────────────────────────────────────────────
// Declared in Value.h; defined here because Environment is not yet complete
// when Value.h is being parsed.
// EZFunction holds closure/staticEnv as shared_ptr<Environment>, NOT as Value,
// so there are no Value edges to traverse. (Environment scope chains cannot
// form GC-relevant cycles because they are always rooted by the call stack.)
void EZFunction::traverse(const ValueVisitor& /*visit*/) const {}

// ── Environment::createChild() ───────────────────────────────────────────────
std::shared_ptr<Environment> Environment::createChild() {
    return std::make_shared<Environment>(shared_from_this());
}
