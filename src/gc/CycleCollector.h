#ifndef CYCLE_COLLECTOR_H
#define CYCLE_COLLECTOR_H

#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <chrono>
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

        // Answer a pending safepoint before taking any lock. A thread that
        // parked while holding mutex_ would block the very collection it is
        // waiting for.
        pollSafepoint();

        bool wantMinor = false;
        bool wantMajor = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            young_tracked_.push_back({ std::static_pointer_cast<void>(ptr), type });
            if (!disabled_) {
                if (young_tracked_.size() >= minor_threshold_)      wantMinor = true;
                else if (old_tracked_.size() >= major_threshold_)   wantMajor = true;
            }
        }
        if (!wantMinor && !wantMajor) return;

        // Back off after a failed handshake. The threshold stays exceeded, so
        // without this every subsequent allocation would retry immediately and
        // pay the full timeout again -- turning one unreachable thread into a
        // hard slowdown for everyone. Retrying after a few thousand
        // allocations costs nothing and still collects promptly once the
        // blocking thread frees up.
        if (safepointBackoff_.load(std::memory_order_relaxed) > 0) {
            safepointBackoff_.fetch_sub(1, std::memory_order_relaxed);
            return;
        }

        // Stop the other mutators, then collect with mutex_ held. The handshake
        // runs with mutex_ RELEASED so a worker can still finish its own
        // track() and get back to a poll; holding it would deadlock the two
        // against each other until the timeout.
        if (!bringToSafepoint()) {
            safepointBackoff_.store(4096, std::memory_order_relaxed);
            return;                         // someone is unreachable: try later
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            // Re-check: another thread may have collected while we waited.
            if (wantMinor && young_tracked_.size() >= minor_threshold_)     collect_minor();
            else if (wantMajor && old_tracked_.size() >= major_threshold_)  collect_major();
        }
        releaseSafepoint();
    }

    // Manual trigger (e.g. from tests or shutdown).
    void collect();

    // Register/unregister a concurrent mutator thread (e.g. a spawn() worker).
    // Locking mutex_ makes registration rendezvous with any in-flight collect():
    // a starting thread blocks until the current collection finishes, and a
    // collection cannot begin once a thread has registered — closing the window
    // where a mutator could start racing mid-collection.
    void beginMutatorThread() { std::lock_guard<std::mutex> lock(mutex_); ++activeMutators_; }
    void endMutatorThread()   {
        { std::lock_guard<std::mutex> lock(mutex_); --activeMutators_; }
        // A thread leaving may be the last one a pending safepoint was waiting
        // for, so wake the collector rather than let it sit out its timeout.
        parkedCv_.notify_all();
    }

    // ── Safepoints ────────────────────────────────────────────────────────────
    // The collector cannot run while another thread is executing bytecode: its
    // phases read use_count() and walk a graph that thread may be reshaping.
    // The old resolution was to skip collection entirely whenever a spawn()
    // worker was alive, which is safe but means a continuously busy pool never
    // collects at all -- measured at 40,000 cyclic objects accumulated and zero
    // reclaimed while six overlapping workers ran.
    //
    // Instead the collector now ASKS the other threads to stop. Each of them
    // polls this flag at backward jumps -- every loop and every recursive call
    // passes one, so any thread running EZ code reaches a poll promptly -- and
    // parks until the collection finishes.
    //
    // pollSafepoint() is the fast path: one relaxed atomic load, no barrier, no
    // lock. It sits on the hot dispatch path so it has to cost nothing when no
    // collection is pending, which is essentially always.
    // True if a collection is waiting for this thread to stop. Callers on the
    // hot dispatch path test this first and only then do the work of making VM
    // state consistent, so the common case costs one relaxed load.
    bool safepointPending() const {
        return safepointRequested_.load(std::memory_order_relaxed);
    }

    void pollSafepoint() {
        if (__builtin_expect(safepointPending(), 0)) {
            parkAtSafepoint();
        }
    }

    // A thread about to block in a native call that does NOT touch the object
    // graph -- sleeping, waiting on a channel, blocking on a socket -- counts as
    // parked for the duration. Without this a worker asleep in wait() would
    // never reach a backward jump, so it could not answer a safepoint request
    // and every collection would time out waiting for it.
    void enterSafeRegion();
    void exitSafeRegion();

    // GC Control Flags (Python-like)
    void disable() { std::lock_guard<std::mutex> lock(mutex_); disabled_ = true; }
    void enable()  { std::lock_guard<std::mutex> lock(mutex_); disabled_ = false; }

    // Statistics
    size_t trackedCount()     const { std::lock_guard<std::mutex> lock(mutex_); return young_tracked_.size() + old_tracked_.size(); }
    size_t cyclesCollected()  const { std::lock_guard<std::mutex> lock(mutex_); return cyclesCollected_; }
    // The major threshold is also raised automatically to track the surviving
    // heap (see collect_major), so what is set here acts as the FLOOR it will
    // never drop below rather than a fixed trigger point.
    void   setThresholds(size_t minor, size_t major) {
        std::lock_guard<std::mutex> lock(mutex_);
        minor_threshold_      = minor;
        major_threshold_base_ = major;
        major_threshold_      = major;
    }

private:
    CycleCollector() = default;

    // Slow path of pollSafepoint(): block until the collection finishes.
    void parkAtSafepoint();

    // Ask every other mutator to park, and wait for them. Returns false if they
    // did not all arrive within the timeout, in which case NOTHING is held and
    // the caller must skip collecting -- degrading to the old defer-and-retry
    // behaviour rather than risking a collection racing a live mutator. A
    // thread stuck in a native call that is not marked as a safe region is the
    // case this covers.
    bool bringToSafepoint();
    void releaseSafepoint();

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

    // Report every candidate-eligible object a Value refers to, looking THROUGH
    // wrapper types that are never tracked themselves (bound methods). Without
    // that expansion an edge landing on a bound method simply stopped, so an
    // instance holding a method bound to itself never looked cyclic.
    static void forEachReferencedPtr(const Value& v,
                                     const std::function<void(void*)>& sink);

    // Null out all outgoing Value references in an object (break cycles).
    void clearObject(
        const std::shared_ptr<void>& obj,
        ValueType                    type);

    // ── State ─────────────────────────────────────────────────────────────────
    mutable std::mutex         mutex_;
    std::vector<TrackedObject> young_tracked_;
    std::vector<TrackedObject> old_tracked_;
    size_t                     minor_threshold_ = 2000;
    // Current trigger point for a major collection, and the floor it is never
    // lowered past. major_threshold_ grows with the surviving heap after each
    // major collection -- see collect_major() for why a fixed value made
    // allocation quadratic.
    size_t                     major_threshold_      = 10000;
    size_t                     major_threshold_base_ = 10000;
    size_t                     cyclesCollected_ = 0;
    bool                       disabled_        = false;
    // Number of live mutator threads (main thread counts as 1).
    std::atomic<int>           activeMutators_{1};

    // ── Safepoint state ───────────────────────────────────────────────────────
    // Guarded by safepointMutex_, which is deliberately SEPARATE from mutex_:
    // the handshake happens with mutex_ released, because a worker that is
    // blocked waiting for mutex_ inside track() cannot also be parking, and
    // holding both would let the collector wait on threads it is itself
    // blocking.
    std::atomic<bool>          safepointRequested_{false};
    std::atomic<int>           parkedMutators_{0};
    // Allocations to skip before retrying a handshake that just timed out.
    std::atomic<int>           safepointBackoff_{0};
    mutable std::mutex         safepointMutex_;
    std::condition_variable    parkedCv_;   // collector waits for threads to arrive
    std::condition_variable    resumeCv_;   // threads wait for the all-clear
};

// RAII wrapper for a blocking native call that does not touch the object graph.
// While it is in scope the thread counts as parked, so the collector need not
// wait for it -- and on the way out the thread blocks until any collection that
// started meanwhile has finished, so it never resumes bytecode mid-collection.
//
//     GCSafeRegion safe;                 // sleeping / waiting on a channel /
//     std::this_thread::sleep_for(...);  // blocked on a socket
//
// Do NOT use it around anything that reads or writes EZ values.
struct GCSafeRegion {
    GCSafeRegion()  { CycleCollector::instance().enterSafeRegion(); }
    ~GCSafeRegion() { CycleCollector::instance().exitSafeRegion(); }
    GCSafeRegion(const GCSafeRegion&)            = delete;
    GCSafeRegion& operator=(const GCSafeRegion&) = delete;
};

#endif // CYCLE_COLLECTOR_H
