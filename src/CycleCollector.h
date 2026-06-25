#ifndef CYCLE_COLLECTOR_H
#define CYCLE_COLLECTOR_H

#include <memory>
#include <vector>
#include <mutex>
#include <functional>
#include <cstdint>

// Forward declarations — Value.h includes us, so we can't include it.
struct Value;
enum class ValueType;

// ─────────────────────────────────────────────────────────────────────────────
// ValueVisitor — the signature passed to traverse()
// ─────────────────────────────────────────────────────────────────────────────
using ValueVisitor = std::function<void(const Value&)>;

// ─────────────────────────────────────────────────────────────────────────────
// TrackedObject — one weak_ptr entry per tracked container.
// We store weak_ptr<void> and the ValueType tag so we can cast back later
// without template proliferation.  weak_ptr<void> does NOT prevent normal
// shared_ptr destruction — when use_count drops to zero naturally, the
// weak_ptr expires and phase1 removes the entry.
// ─────────────────────────────────────────────────────────────────────────────
struct TrackedObject {
    std::weak_ptr<void> weakRef;
    ValueType           type;
};

// ─────────────────────────────────────────────────────────────────────────────
// CycleCollector — singleton cycle-detection GC.
//
// Responsibility:  find objects that are ONLY referenced by other objects in
//                  the same reference cycle so shared_ptr refcounting alone
//                  would never free them.
//
// Algorithm:  Bacon & Rajan (2001) reference-count adjustment.
//             No root-set scanning, no Stop-The-World, no safepoints.
//
// Usage:
//   1. Call track() once when a container object is first constructed.
//   2. Rely on shared_ptr for all other lifetime management.
//   3. Collect() is triggered automatically when tracked_.size() >= threshold_,
//      or can be called manually.
// ─────────────────────────────────────────────────────────────────────────────
class CycleCollector {
public:
    static CycleCollector& instance() {
        static CycleCollector cc;
        return cc;
    }

    // Register a freshly-constructed container. Call ONCE per construction,
    // never on copy/move (shared_ptr copy doesn't create a new object).
    template<typename T>
    void track(const std::shared_ptr<T>& ptr, ValueType type) {
        if (!ptr) return;
        std::lock_guard<std::mutex> lock(mutex_);
        tracked_.push_back({ std::static_pointer_cast<void>(ptr), type });
        if (tracked_.size() >= threshold_) {
            collect_locked();
        }
    }

    // Manual trigger (e.g. from tests or shutdown).
    void collect();

    // Statistics
    size_t trackedCount()     const { return tracked_.size(); }
    size_t cyclesCollected()  const { return cyclesCollected_; }
    void   setThreshold(size_t n)   { threshold_ = n; }

private:
    CycleCollector() = default;

    void collect_locked();

    // ── Four-phase algorithm ──────────────────────────────────────────────────

    // Phase 1: drop weak_ptrs that already expired (freed by refcount naturally)
    void phase1_purgeExpired();

    // Phase 2: snapshot live candidates and compute adjusted reference counts.
    //   adjusted[i] = use_count(i) - (number of references TO i FROM other candidates)
    void phase2_buildCandidates(
        std::vector<std::shared_ptr<void>>& live,
        std::vector<ValueType>&             types,
        std::vector<int>&                   adjustedRC);

    // Phase 3: flood-fill from externally-reachable candidates (adjustedRC > 0)
    //   to find the full live set within candidates. Unmarked == cyclic garbage.
    std::vector<size_t> phase3_findGarbage(
        const std::vector<std::shared_ptr<void>>& live,
        const std::vector<ValueType>&             types,
        std::vector<int>&                         adjustedRC);

    // Phase 4: break outgoing references on garbage objects, letting
    //   shared_ptr destructors fire normally once our local copies drop.
    void phase4_breakCycles(
        const std::vector<std::shared_ptr<void>>& live,
        const std::vector<ValueType>&             types,
        const std::vector<size_t>&                garbageIndices);

    // ── Helpers ───────────────────────────────────────────────────────────────

    // Call traverse() on a typed void-ptr object.
    void traverseObject(
        const std::shared_ptr<void>& obj,
        ValueType                    type,
        ValueVisitor&                visitor) const;

    // Extract a raw void* from a Value for pointer-identity lookup.
    static void* extractRawPtr(const Value& v);

    // Null out all outgoing Value references in an object (break cycles).
    void clearObject(
        const std::shared_ptr<void>& obj,
        ValueType                    type);

    // ── State ─────────────────────────────────────────────────────────────────
    std::mutex                  mutex_;
    std::vector<TrackedObject>  tracked_;
    size_t                      threshold_      = 700;
    size_t                      cyclesCollected_ = 0;
};

#endif // CYCLE_COLLECTOR_H
