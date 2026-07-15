#include "runtime/objects/EZObjects.h"
#include "gc/CycleCollector.h"
#include "runtime/objects/EZUpvalue.h"

void EZClosure::traverse(const ValueVisitor& visit) const {
    for (const auto& uv : upvalues) {
        if (uv) visit(uv->closed);
    }
}
#include "runtime/Value.h"          // Full definition of Value, EZArray, EZInstance, etc.
#include <algorithm>
#include <unordered_map>

// ─────────────────────────────────────────────────────────────────────────────
// extractRawPtr — extract the raw heap pointer from a Value for identity lookup
// ─────────────────────────────────────────────────────────────────────────────
void* CycleCollector::extractRawPtr(const Value& v) {
    switch (v.type()) {
        case ValueType::ARRAY:        return v.asArrayPtr().get();
        case ValueType::TUPLE:        return v.asTuplePtr().get();
        case ValueType::INSTANCE:     return v.asInstance().get();
        case ValueType::DICTIONARY:   return v.asDictionaryPtr().get();
        case ValueType::CLASS:        return v.asClass().get();
        case ValueType::CLOSURE_VAL:  return v.asClosure().get();
        case ValueType::BOUND_METHOD: return v.asBoundMethod().get();
        case ValueType::CONCAT_STRING:return v.asConcatStringPtr().get();
        case ValueType::FUNCTION:     return v.asFunction().get();
        default:                      return nullptr;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// traverseObject — call traverse() on a typed void-ptr
// ─────────────────────────────────────────────────────────────────────────────
void CycleCollector::traverseObject(
    const std::shared_ptr<void>& obj,
    ValueType                    type,
    ValueVisitor&                visitor) const
{
    switch (type) {
        case ValueType::ARRAY:
            std::static_pointer_cast<EZArray>(obj)->traverse(visitor);
            break;
        case ValueType::TUPLE:
            std::static_pointer_cast<EZTuple>(obj)->traverse(visitor);
            break;
        case ValueType::DICTIONARY:
            std::static_pointer_cast<EZDictionary>(obj)->traverse(visitor);
            break;
        case ValueType::INSTANCE:
            std::static_pointer_cast<EZInstance>(obj)->traverse(visitor);
            break;
        case ValueType::CLASS:
            std::static_pointer_cast<EZClass>(obj)->traverse(visitor);
            break;
        case ValueType::FUNCTION:
            std::static_pointer_cast<EZFunction>(obj)->traverse(visitor);
            break;
        case ValueType::CLOSURE_VAL:
            std::static_pointer_cast<EZClosure>(obj)->traverse(visitor);
            break;
        case ValueType::BOUND_METHOD:
            std::static_pointer_cast<EZBoundMethod>(obj)->traverse(visitor);
            break;
        case ValueType::CONCAT_STRING:
            std::static_pointer_cast<EZConcatString>(obj)->traverse(visitor);
            break;
        default:
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// clearObject — break all outgoing Value references to allow shared_ptr
//               refcounts to drop to zero and destructors to fire.
// ─────────────────────────────────────────────────────────────────────────────
void CycleCollector::clearObject(
    const std::shared_ptr<void>& obj,
    ValueType                    type)
{
    switch (type) {
        case ValueType::ARRAY: {
            auto arr = std::static_pointer_cast<EZArray>(obj);
            arr->modifyElements([&](auto& e) { e.clear(); });
            break;
        }
        case ValueType::TUPLE: {
            auto tup = std::static_pointer_cast<EZTuple>(obj);
            tup->modifyElements([&](auto& e) { e.clear(); });
            break;
        }
        case ValueType::DICTIONARY: {
            auto dict = std::static_pointer_cast<EZDictionary>(obj);
            dict->modifyMap([&](auto& m) { m.clear(); });
            break;
        }
        case ValueType::INSTANCE: {
            auto inst = std::static_pointer_cast<EZInstance>(obj);
            inst->modifyProperties([&](auto& p) { p.clear(); });
            inst->klass = nullptr;
            if (inst->getAuditLog())   inst->getAuditLog()->clear();
            if (inst->getCacheStore()) inst->getCacheStore()->clear();
            break;
        }
        case ValueType::CLASS: {
            auto klass = std::static_pointer_cast<EZClass>(obj);
            klass->methods.clear();
            klass->staticMembers.clear();
            klass->parent = nullptr;
            for (auto& v : klass->validators) v.param = Value();
            break;
        }
        case ValueType::FUNCTION: {
            auto func = std::static_pointer_cast<EZFunction>(obj);
            func->closure   = nullptr;
            func->staticEnv = nullptr;
            func->bytecode  = nullptr;
            break;
        }
        case ValueType::CLOSURE_VAL: {
            auto clos = std::static_pointer_cast<EZClosure>(obj);
            clos->upvalues.clear();
            clos->function = nullptr;
            break;
        }
        case ValueType::BOUND_METHOD: {
            auto bm = std::static_pointer_cast<EZBoundMethod>(obj);
            bm->receiver = Value();
            bm->method   = Value();
            break;
        }
        case ValueType::CONCAT_STRING: {
            auto cs = std::static_pointer_cast<EZConcatString>(obj);
            cs->left      = Value();
            cs->right     = Value();
            cs->flattened = nullptr;
            break;
        }
        default:
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 1: Remove expired weak_ptrs (already freed by refcount — not cycles)
// ─────────────────────────────────────────────────────────────────────────────
void CycleCollector::phase1_purgeExpired(std::vector<TrackedObject>& buffer) {
    buffer.erase(
        std::remove_if(buffer.begin(), buffer.end(),
            [](const TrackedObject& to) { return to.weakRef.expired(); }),
        buffer.end());
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 2: Build candidate snapshot and compute adjusted reference counts.
//
// For each candidate A: adjustedRC[A] = use_count(A) - (refs to A from other candidates).
// If adjustedRC[A] == 0 after subtracting internal refs, A is only kept alive
// by the cycle itself — potential garbage.
// ─────────────────────────────────────────────────────────────────────────────
void CycleCollector::phase2_buildCandidates(
    const std::vector<TrackedObject>&   candidates,
    std::vector<std::shared_ptr<void>>& live,
    std::vector<ValueType>&             types,
    std::vector<int>&                   adjustedRC)
{
    live.reserve(candidates.size());
    types.reserve(candidates.size());
    adjustedRC.reserve(candidates.size());

    for (auto& to : candidates) {
        auto sp = to.weakRef.lock();
        if (!sp) continue;
        // Read use_count BEFORE storing another copy in live[]. At this point the
        // only collector-owned reference is the local `sp`, so subtracting 1 yields
        // the object's true external reference count. (Reading after push_back
        // would count the live[] copy too, inflating every adjustedRC by 1 and
        // preventing any cycle from ever being collected.)
        int externalRC = static_cast<int>(sp.use_count()) - 1;
        live.push_back(sp);
        types.push_back(to.type);
        adjustedRC.push_back(externalRC);
    }

    // Build pointer → index map for O(1) lookup during traversal.
    std::unordered_map<void*, size_t> ptrToIdx;
    ptrToIdx.reserve(live.size());
    for (size_t i = 0; i < live.size(); i++) {
        ptrToIdx[live[i].get()] = i;
    }

    // For each candidate, traverse its outgoing refs.
    // For every ref that points to another candidate, decrement that
    // candidate's adjustedRC (because that ref is internal to the set).
    for (size_t i = 0; i < live.size(); i++) {
        ValueVisitor visitor = [&](const Value& v) {
            void* raw = extractRawPtr(v);
            if (raw) {
                auto it = ptrToIdx.find(raw);
                if (it != ptrToIdx.end()) {
                    adjustedRC[it->second]--;
                }
            }
        };
        traverseObject(live[i], types[i], visitor);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 3: Flood-fill from externally-reachable candidates (adjustedRC > 0)
//          to find the full live set. Unmarked candidates = cyclic garbage.
// ─────────────────────────────────────────────────────────────────────────────
std::vector<size_t> CycleCollector::phase3_findGarbage(
    const std::vector<std::shared_ptr<void>>& live,
    const std::vector<ValueType>&             types,
    std::vector<int>&                         adjustedRC)
{
    std::vector<bool> liveFlag(live.size(), false);

    // Seed: any candidate with adjustedRC > 0 is reachable from outside.
    for (size_t i = 0; i < live.size(); i++) {
        if (adjustedRC[i] > 0) liveFlag[i] = true;
    }

    // Build ptr → index map.
    std::unordered_map<void*, size_t> ptrToIdx;
    ptrToIdx.reserve(live.size());
    for (size_t i = 0; i < live.size(); i++) {
        ptrToIdx[live[i].get()] = i;
    }

    // Flood-fill: everything reachable from a live candidate is also live.
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < live.size(); i++) {
            if (!liveFlag[i]) continue;
            ValueVisitor visitor = [&](const Value& v) {
                void* raw = extractRawPtr(v);
                if (raw) {
                    auto it = ptrToIdx.find(raw);
                    if (it != ptrToIdx.end() && !liveFlag[it->second]) {
                        liveFlag[it->second] = true;
                        changed = true;
                    }
                }
            };
            traverseObject(live[i], types[i], visitor);
        }
    }

    // Collect garbage indices.
    std::vector<size_t> garbage;
    for (size_t i = 0; i < live.size(); i++) {
        if (!liveFlag[i]) garbage.push_back(i);
    }
    return garbage;
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 4: Break outgoing refs on garbage objects. shared_ptr destructors
//          will fire as our local 'live' copies drop at end of collect_locked().
// ─────────────────────────────────────────────────────────────────────────────
void CycleCollector::phase4_breakCycles(
    const std::vector<std::shared_ptr<void>>& live,
    const std::vector<ValueType>&             types,
    const std::vector<size_t>&                garbageIndices)
{
    for (size_t idx : garbageIndices) {
        clearObject(live[idx], types[idx]);
        cyclesCollected_++;
    }
    // live[] drops here → destructors fire where refcount reaches zero.
}

// ─────────────────────────────────────────────────────────────────────────────
// collect_locked — run full collection. Caller must hold mutex_.
// ─────────────────────────────────────────────────────────────────────────────
void CycleCollector::collect_internal(std::vector<TrackedObject>& candidates, bool is_major) {
    phase1_purgeExpired(candidates);
    if (candidates.empty()) return;

    std::vector<std::shared_ptr<void>> live;
    std::vector<ValueType>             types;
    std::vector<int>                   adjustedRC;

    phase2_buildCandidates(candidates, live, types, adjustedRC);
    auto garbage = phase3_findGarbage(live, types, adjustedRC);
    phase4_breakCycles(live, types, garbage);
    
    // Purge the expired entries resulting from phase4 broken cycles
    phase1_purgeExpired(candidates);
}

void CycleCollector::collect_minor() {
    collect_internal(young_tracked_, false);
    // Promote survivors to old generation
    old_tracked_.insert(old_tracked_.end(), young_tracked_.begin(), young_tracked_.end());
    young_tracked_.clear();
}

void CycleCollector::collect_major() {
    // Combine old and young
    std::vector<TrackedObject> combined = old_tracked_;
    combined.insert(combined.end(), young_tracked_.begin(), young_tracked_.end());
    
    collect_internal(combined, true);
    
    // All survivors form the new old generation
    old_tracked_ = std::move(combined);
    young_tracked_.clear();
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────
void CycleCollector::collect() {
    std::lock_guard<std::mutex> lock(mutex_);
    // Refuse to collect while another mutator thread is live — collecting then
    // would race concurrent mutation (see track()/beginMutatorThread()). The
    // garbage stays tracked and is reclaimed by a later collect once the extra
    // threads have joined.
    if (activeMutators_.load(std::memory_order_acquire) != 1) return;
    collect_major();
}
