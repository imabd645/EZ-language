#ifndef EZCONCATSTRING_H
#define EZCONCATSTRING_H

#include "runtime/Value.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <mutex>

struct EZConcatString {
    Value left;
    Value right;
    size_t length = 0;
    bool isFlattened = false;
    std::shared_ptr<std::string> flattened;

    void traverse(const ValueVisitor& visit) const {
        visit(left);
        visit(right);
    }

    // `s = s + "a"` in a tight loop builds a left-leaning concat tree one
    // level deeper per iteration (see doAdd(): every string `+` allocates a
    // new EZConcatString wrapping the previous value as `left`, unconditionally,
    // with no eager flattening or depth limit). That chain isn't torn down
    // incrementally -- each iteration's node is kept alive as `left` of the
    // next one -- so the whole thing comes down in one shot whenever the
    // final `s` is destroyed (function return, reassignment, GC sweep).
    // Without this destructor, that's the *default* one: ~Value() on `left`
    // drops a shared_ptr<EZConcatString> refcount to zero, which invokes
    // ~EZConcatString() again, recursing one C++ stack frame per level of
    // the chain. A large enough loop overflows the OS stack with no EZ-level
    // traceback -- just a hard crash. (A release build's optimizer can
    // sometimes turn the single-branch recursion below into a loop and mask
    // this, which is exactly why it isn't safe to leave unfixed: whether it
    // crashes becomes a function of compiler flags, not program correctness.)
    //
    // Fix: detach children before they'd be recursively destroyed, and walk
    // the chain with an explicit worklist instead of the call stack.
    ~EZConcatString() {
        std::vector<Value> pending;
        // `use_count() == 2` (not 1): asConcatStringPtr() returns the
        // shared_ptr by value, so `cs` here is itself a second owner for as
        // long as this lambda invocation is on the stack. Checking against 1
        // would never be true -- it'd count `cs`'s own temporary copy against
        // itself -- and silently turn this whole fix into a no-op that still
        // recurses via the plain `v = Value()` below.
        auto detach = [&](Value& v) {
            if (v.type() == ValueType::CONCAT_STRING) {
                auto cs = v.asConcatStringPtr();
                if (cs.use_count() == 2) {
                    pending.push_back(std::move(v));
                }
            }
            v = Value();
        };
        detach(left);
        detach(right);
        while (!pending.empty()) {
            Value v = std::move(pending.back());
            pending.pop_back();
            auto cs = v.asConcatStringPtr();
            // `cs` and `v` are the only two owners at this point (that's what
            // got it onto the worklist). Detach ITS children the same way
            // before `v` (and this local `cs`) go out of scope and destroy
            // the pointee -- by then its left/right are already nulled out,
            // so that destruction is O(1) instead of recursing further.
            detach(cs->left);
            detach(cs->right);
        }
    }
};

// --- Container structs (owned by shared_ptr, tracked by CycleCollector) ---


#endif // EZCONCATSTRING_H
