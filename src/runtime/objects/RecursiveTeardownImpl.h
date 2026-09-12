#ifndef EZ_RECURSIVE_TEARDOWN_IMPL_H
#define EZ_RECURSIVE_TEARDOWN_IMPL_H

#include "RecursiveTeardown.h"

// See RecursiveTeardown.h for why this exists and why it's split into a
// forward declaration (included early) and this implementation (included
// last, once EZArray/EZTuple/EZDictionary/EZInstance/EZConcatString are all
// fully defined -- this needs their actual members, not just Value's view
// of them as opaque shared_ptrs).

namespace ez_detail {

inline bool ezIsRecursiveContainer(const Value& v) {
    switch (v.type()) {
        case ValueType::CONCAT_STRING:
        case ValueType::ARRAY:
        case ValueType::TUPLE:
        case ValueType::DICTIONARY:
        case ValueType::INSTANCE:
            return true;
        default:
            return false;
    }
}

// True if `v` is the only remaining owner of its underlying object. Each
// asXxxPtr() call below returns the held shared_ptr *by value*, so the
// local copy it's invoked on is itself a second owner for the life of that
// one expression -- checking against 2 (not 1) is deliberate, not an
// off-by-one: 1 would count that temporary against itself and this check
// would never pass, silently turning the whole mechanism into a no-op that
// still recurses via the plain assignment in detach() below.
inline bool ezIsSoleOwner(const Value& v) {
    switch (v.type()) {
        case ValueType::CONCAT_STRING: return v.asConcatStringPtr().use_count() == 2;
        case ValueType::ARRAY:         return v.asArrayPtr().use_count() == 2;
        case ValueType::TUPLE:         return v.asTuplePtr().use_count() == 2;
        case ValueType::DICTIONARY:    return v.asDictionaryPtr().use_count() == 2;
        case ValueType::INSTANCE:      return v.asInstance().use_count() == 2;
        default: return false;
    }
}

// Appends pointers to `v`'s own Value-holding children (if it holds a
// recognized container type; does nothing otherwise). The returned pointers
// stay valid as long as `v` (or any other owner) keeps the underlying
// object alive, which holds for the immediate, synchronous use this is put
// to below.
inline void ezAppendContainerChildren(const Value& v, std::vector<Value*>& out) {
    switch (v.type()) {
        case ValueType::CONCAT_STRING: {
            auto p = v.asConcatStringPtr();
            out.push_back(&p->left);
            out.push_back(&p->right);
            break;
        }
        case ValueType::ARRAY: {
            auto p = v.asArrayPtr();
            p->appendChildPointers(out);
            break;
        }
        case ValueType::TUPLE: {
            auto p = v.asTuplePtr();
            p->appendChildPointers(out);
            break;
        }
        case ValueType::DICTIONARY: {
            auto p = v.asDictionaryPtr();
            p->appendChildPointers(out);
            break;
        }
        case ValueType::INSTANCE: {
            auto p = v.asInstance();
            for (auto& pv : p->propertyValues) out.push_back(&pv);
            break;
        }
        default:
            break;
    }
}

} // namespace ez_detail

inline void ezIterativelyReleaseChildren(std::vector<Value*> children) {
    std::vector<Value> pending;
    auto detach = [&](Value* slot) {
        if (ez_detail::ezIsRecursiveContainer(*slot) && ez_detail::ezIsSoleOwner(*slot)) {
            pending.push_back(std::move(*slot));
        }
        *slot = Value();
    };
    for (Value* slot : children) detach(slot);

    while (!pending.empty()) {
        Value v = std::move(pending.back());
        pending.pop_back();
        // `v` is (by construction, from the check above) the sole owner of
        // its underlying object. Detach ITS children the same way before it
        // goes out of scope at the end of this loop body: by then they're
        // already nulled out, so the destruction that fires here is O(1)
        // instead of recursing into a chain of arbitrary depth.
        std::vector<Value*> grandchildren;
        ez_detail::ezAppendContainerChildren(v, grandchildren);
        for (Value* slot : grandchildren) detach(slot);
    }
}

#endif // EZ_RECURSIVE_TEARDOWN_IMPL_H
