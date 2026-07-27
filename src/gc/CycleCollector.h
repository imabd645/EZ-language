#ifndef CYCLE_COLLECTOR_H
#define CYCLE_COLLECTOR_H

#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
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
        young_tracked_.push_back({ std::static_pointer_cast<void>(ptr), type });
        // Only run collection when this is the sole active mutator thread.
        // The Bacon-Rajan phases read use_count() and traverse/clear object
        // graphs that other spawn()ed VM threads mutate concurrently without
        // any synchronisation, so collecting while another thread is live would
        // race (misclassified garbage -> use-after-free / wiped-live-object).
        // Deferring is safe: the objects stay tracked and are reclaimed once
        // the extra threads join (activeMutators_ returns to 1).
        if (!disabled_ && activeMutators_.load(std::memory_order_acquire) == 1) {
            if (young_tracked_.size() >= minor_threshold_) {
                collect_minor();
            } else if (old_tracked_.size() >= major_threshold_) {
                collect_major();
            }
        }
    }

    // Manual trigger (e.g. from tests or shutdown).
    void collect();

    // Register/unregister a concurrent mutator thread (e.g. a spawn() worker).
    // Locking mutex_ makes registration rendezvous with any in-flight collect():
    // a starting thread blocks until the current collection finishes, and a
    // collection cannot begin once a thread has registered — closing the window
    // where a mutator could start racing mid-collection.
    void beginMutatorThread() { std::lock_guard<std::mutex> lock(mutex_); ++activeMutators_; }
    void endMutatorThread()   { std::lock_guard<std::mutex> lock(mutex_); --activeMutators_; }

    // GC Control Flags (Python-like)
    void disable() { std::lock_guard<std::mutex> lock(mutex_); disabled_ = true; }
    void enable()  { std::lock_guard<std::mutex> lock(mutex_); disabled_ = false; }

    // Statistics
    size_t trackedCount()     const { std::lock_guard<std::mutex> lock(mutex_); return young_tracked_.size() + old_tracked_.size(); }
    size_t cyclesCollected()  const { std::lock_guard<std::mutex> lock(mutex_); return cyclesCollected_; }
    void   setThresholds(size_t minor, size_t major) { minor_threshold_ = minor; major_threshold_ = major; }

private:
    CycleCollector() = default;

    void collect_minor();
    void collect_major();
    void collect_internal(std::vector<TrackedObject>& candidates, bool is_major);

    // ── Four-phase algorithm ──────────────────────────────────────────────────

    // Phase 1: drop weak_ptrs that already expired (freed by refcount naturally)
    void phase1_purgeExpired(std::vector<TrackedObject>& buffer);

    // Phase 2: snapshot live candidates and compute adjusted reference counts.
    //   adjusted[i] = use_count(i) - (number of references TO i FROM other candidates)
    void phase2_buildCandidates(
        const std::vector<TrackedObject>&   candidates,
        std::vector<std::shared_ptr<void>>& live,
        std::vector<ValueType>&             types,
        std::vector<int>&                   adjustedRC,
        std::unordered_map<void*, size_t>&  ptrToIdx);

    // Phase 3: flood-fill from externally-reachable candidates (adjustedRC > 0)
    //   to find the full live set within candidates. Unmarked == cyclic garbage.
    std::vector<size_t> phase3_findGarbage(
        const std::vector<std::shared_ptr<void>>& live,
        const std::vector<ValueType>&             types,
        std::vector<int>&                         adjustedRC,
        const std::unordered_map<void*, size_t>&  ptrToIdx);

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
    mutable std::mutex         mutex_;
    std::vector<TrackedObject> young_tracked_;
    std::vector<TrackedObject> old_tracked_;
    size_t                     minor_threshold_ = 2000;
    size_t                     major_threshold_ = 10000;
    size_t                     cyclesCollected_ = 0;
    bool                       disabled_        = false;
    // Number of live mutator threads (main thread counts as 1). Collection only
    // runs when this is 1, i.e. no spawn() worker is concurrently mutating.
    std::atomic<int>           activeMutators_{1};
};

#endif // CYCLE_COLLECTOR_H
