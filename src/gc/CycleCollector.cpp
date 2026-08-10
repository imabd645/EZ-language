#include "runtime/objects/EZObjects.h"
#include "gc/CycleCollector.h"
#include "runtime/objects/EZUpvalue.h"

void EZClosure::traverse(const ValueVisitor& visit) const {
    for (const auto& uv : upvalues) {
        if (!uv) continue;
        // Follow the CURRENT location, not just `closed`. An upvalue that is
        // still open points at a live stack slot and its `closed` field is nil,
        // so visiting only `closed` made the captured value invisible to the
        // collector -- a closure that captures the container holding it looked
        // acyclic and was never reclaimed.
        //
        // Reading another thread's stack slot is safe here because collection
        // only runs at a safepoint, with every other mutator parked.
        Value* loc = uv->location.load(std::memory_order_acquire);
        visit(loc ? *loc : uv->closed);
    }
}
#include "runtime/Value.h"          // Full definition of Value, EZArray, EZInstance, etc.
#include <algorithm>
#include <unordered_map>

// ─────────────────────────────────────────────────────────────────────────────
// extractRawPtr — extract the raw heap pointer from a Value for identity lookup
// ─────────────────────────────────────────────────────────────────────────────
// Report every candidate-eligible object a Value refers to, looking THROUGH
// wrapper types that are not themselves tracked.
//
// A bound method is the case that matters. It is never registered with the
// collector, so it can never be a candidate, and an edge that lands on one used
// to stop there -- making `n.fn = n.hello` (an instance holding a method bound
// to itself) look like a reference to something outside the candidate set. The
// instance therefore kept a positive adjusted count and was never collected.
// Expanding through the wrapper turns that into the self-edge it really is.
void CycleCollector::forEachReferencedPtr(const Value& v,
                                          const std::function<void(void*)>& sink) {
    if (v.type() == ValueType::BOUND_METHOD) {
        auto bm = v.asBoundMethod();
        if (bm) {
            // The wrapper itself is not a candidate; its two halves may be.
            forEachReferencedPtr(bm->receiver, sink);
            forEachReferencedPtr(bm->method, sink);
        }
        return;
    }
    if (void* raw = extractRawPtr(v)) sink(raw);
}

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
    std::vector<int>&                   adjustedRC,
    std::unordered_map<void*, size_t>&  ptrToIdx)
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
    ptrToIdx.reserve(live.size());
    for (size_t i = 0; i < live.size(); i++) {
        ptrToIdx[live[i].get()] = i;
    }

    // For each candidate, traverse its outgoing refs.
    // For every ref that points to another candidate, decrement that
    // candidate's adjustedRC (because that ref is internal to the set).
    for (size_t i = 0; i < live.size(); i++) {
        ValueVisitor visitor = [&](const Value& v) {
            forEachReferencedPtr(v, [&](void* raw) {
                auto it = ptrToIdx.find(raw);
                if (it != ptrToIdx.end()) {
                    adjustedRC[it->second]--;
                }
            });
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
    std::vector<int>&                         adjustedRC,
    const std::unordered_map<void*, size_t>&  ptrToIdx)
{
    std::vector<bool> liveFlag(live.size(), false);

    // Seed: any candidate with adjustedRC > 0 is reachable from outside.
    std::vector<size_t> worklist;
    for (size_t i = 0; i < live.size(); i++) {
        if (adjustedRC[i] > 0) {
            liveFlag[i] = true;
            worklist.push_back(i);
        }
    }

    // Flood-fill from the seeds. Each object is traversed at most once, when it
    // is first marked live, so this is O(objects + references).
    //
    // This was previously a fixed point -- `while (changed)` rescanning EVERY
    // candidate on each pass. Liveness then spread only one link per pass, so a
    // chain-shaped heap (`d = [d]` repeated, a linked list, a deep AST) needed
    // as many passes as it had links, each pass costing a full scan: O(N^2).
    // Allocating 5000 nested arrays spent ~490ms in the collector against 2ms
    // with it disabled, and 10000 took 4s. A worklist reaches the same fixed
    // point in one pass over each object.
    //
    // Iterative rather than recursive on purpose: the traversal depth equals
    // the heap's reference depth, which is unbounded from a script's point of
    // view, and recursing on it would exhaust the native stack.
    while (!worklist.empty()) {
        size_t i = worklist.back();
        worklist.pop_back();
        ValueVisitor visitor = [&](const Value& v) {
            forEachReferencedPtr(v, [&](void* raw) {
                auto it = ptrToIdx.find(raw);
                if (it != ptrToIdx.end() && !liveFlag[it->second]) {
                    liveFlag[it->second] = true;
                    worklist.push_back(it->second);
                }
            });
        };
        traverseObject(live[i], types[i], visitor);
    }

    // Collect garbage indices.
    std::vector<size_t> garbage;
    garbage.reserve(live.size());
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
    std::unordered_map<void*, size_t>  ptrToIdx;

    phase2_buildCandidates(candidates, live, types, adjustedRC, ptrToIdx);
    auto garbage = phase3_findGarbage(live, types, adjustedRC, ptrToIdx);
    phase4_breakCycles(live, types, garbage);
    
    // Purge the expired entries resulting from phase4 broken cycles
    phase1_purgeExpired(candidates);
}

// ─────────────────────────────────────────────────────────────────────────────
// Safepoint protocol
//
//   collector                          worker thread
//   ---------                          -------------
//   safepointRequested_ = true
//   wait for parkedMutators_           ... pollSafepoint() at a backward jump
//     to reach the thread count        parkAtSafepoint(): ++parked, notify, wait
//   (all parked) -> collect
//   safepointRequested_ = false
//   notify_all                         wakes, --parked, resumes bytecode
//
// If the workers do not all arrive within the timeout the request is withdrawn
// and no collection happens this round. That is the same outcome as the old
// unconditional deferral, so the worst case is unchanged -- but only for the
// threads that genuinely cannot answer, instead of for every program that ever
// called spawn().
// ─────────────────────────────────────────────────────────────────────────────

void CycleCollector::parkAtSafepoint() {
    std::unique_lock<std::mutex> lk(safepointMutex_);
    // Re-check under the lock: the request may have been withdrawn between the
    // relaxed load in pollSafepoint() and getting here.
    if (!safepointRequested_.load(std::memory_order_acquire)) return;

    parkedMutators_.fetch_add(1, std::memory_order_release);
    parkedCv_.notify_all();
    resumeCv_.wait(lk, [this] {
        return !safepointRequested_.load(std::memory_order_acquire);
    });
    parkedMutators_.fetch_sub(1, std::memory_order_release);
}

bool CycleCollector::bringToSafepoint() {
    const int others = activeMutators_.load(std::memory_order_acquire) - 1;
    if (others <= 0) return true;   // sole mutator: nothing to coordinate

    // Only one collector at a time may drive a safepoint.
    bool expected = false;
    if (!safepointRequested_.compare_exchange_strong(expected, true,
                                                     std::memory_order_acq_rel)) {
        return false;               // another thread is already collecting
    }

    std::unique_lock<std::mutex> lk(safepointMutex_);
    // Bounded: a thread blocked in a native call that is not a marked safe
    // region (a raw FFI call, say) will never arrive, and waiting forever would
    // hang the program rather than merely delay a collection.
    const bool allParked = parkedCv_.wait_for(
        lk, std::chrono::milliseconds(25),
        [this, others] {
            // Threads that exited in the meantime no longer need to park.
            const int stillRunning = activeMutators_.load(std::memory_order_acquire) - 1;
            return parkedMutators_.load(std::memory_order_acquire) >= stillRunning
                || stillRunning <= 0;
        });

    if (!allParked) {
        safepointRequested_.store(false, std::memory_order_release);
        lk.unlock();
        resumeCv_.notify_all();
        return false;
    }
    return true;   // callers must pair this with releaseSafepoint()
}

void CycleCollector::releaseSafepoint() {
    {
        std::lock_guard<std::mutex> lk(safepointMutex_);
        safepointRequested_.store(false, std::memory_order_release);
    }
    resumeCv_.notify_all();
}

void CycleCollector::enterSafeRegion() {
    std::lock_guard<std::mutex> lk(safepointMutex_);
    parkedMutators_.fetch_add(1, std::memory_order_release);
    parkedCv_.notify_all();
}

void CycleCollector::exitSafeRegion() {
    std::unique_lock<std::mutex> lk(safepointMutex_);
    // Stay counted as parked until any collection in flight has finished --
    // leaving early would resume bytecode while the collector is still walking
    // the graph, which is exactly the race safepoints exist to prevent.
    resumeCv_.wait(lk, [this] {
        return !safepointRequested_.load(std::memory_order_acquire);
    });
    parkedMutators_.fetch_sub(1, std::memory_order_release);
}

void CycleCollector::collect_minor() {
    collect_internal(young_tracked_, false);
    // Promote survivors to old generation
    old_tracked_.reserve(old_tracked_.size() + young_tracked_.size());
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

    // Raise the next trigger to sit above the surviving heap.
    //
    // Without this the threshold was fixed, so a program legitimately holding
    // more than major_threshold_ live objects re-entered a FULL major
    // collection on every subsequent allocation: the condition that fired the
    // collection was still true when it finished, because nothing was garbage.
    // Building N live objects therefore cost O(N^2) -- 5000 nested arrays took
    // 528ms versus 2ms with the collector disabled, and 10000 did not finish.
    //
    // Scaling by the survivor count spaces major collections geometrically, so
    // the amortised cost per allocation is constant while the collector still
    // runs often enough to reclaim cycles. This is what CPython's generational
    // GC does with its long-lived-object growth factor.
    const size_t survivors = old_tracked_.size();
    const size_t scaled    = survivors + survivors / 2;   // 1.5x headroom
    major_threshold_ = (scaled > major_threshold_base_) ? scaled : major_threshold_base_;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────
void CycleCollector::collect() {
    // Bring the other mutators to a stop first. This used to return without
    // doing anything whenever a spawn() worker was alive, which made an
    // explicit gc_collect() silently do nothing in exactly the programs that
    // needed it most.
    if (!bringToSafepoint()) return;   // unreachable thread: caller can retry
    {
        std::lock_guard<std::mutex> lock(mutex_);
        collect_major();
    }
    releaseSafepoint();
}
