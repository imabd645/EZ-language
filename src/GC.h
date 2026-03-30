#ifndef GC_H
#define GC_H

#include <vector>
#include <unordered_set>
#include <memory>
#include <atomic>
#include "Value.h"
#include "GCObject.h"

// Simple cycle-detecting garbage collector
class GarbageCollector {
public:
    static GarbageCollector& instance() {
        static GarbageCollector gc;
        return gc;
    }
    
    // Tracking list management
    void registerObject(GCObject* obj) {
        obj->gc_next = head;
        if (head) head->gc_prev = obj;
        head = obj;
    }
    
    void unregisterObject(GCObject* obj) {
        if (obj->gc_prev) obj->gc_prev->gc_next = obj->gc_next;
        if (obj->gc_next) obj->gc_next->gc_prev = obj->gc_prev;
        if (obj == head) head = obj->gc_next;
        
        if (isCollecting) {
            stillValid.erase(obj);
        }
    }
    
    // Track an allocation - registration is handled by GCObject constructor
    template<typename T>
    std::shared_ptr<T> track(std::shared_ptr<T> ptr) {
        return ptr;
    }
    
    // Set root environment for marking
    void setRoot(std::shared_ptr<Environment> root) {
        rootEnv = root;
    }
    
    void addTemporaryRoot(GCObject* obj) {
        tempRoots.insert(obj);
    }
    
    void removeTemporaryRoot(GCObject* obj) {
        tempRoots.erase(obj);
    }
    
    // Cycle detection collection
    void collect(std::shared_ptr<Environment> current = nullptr, 
                 const std::vector<std::shared_ptr<Environment>>* envStack = nullptr);
    
    // Increment count - called by GCObject constructor
    void incrementAllocCount() {
        allocCount++;
    }
    
    // Check if threshold reached and collect
    void collectIfThresholdReached(std::shared_ptr<Environment> current = nullptr,
                                   const std::vector<std::shared_ptr<Environment>>* envStack = nullptr) {
        if (allocCount > gcThreshold) {
            collect(current, envStack);
            allocCount = 0;
        }
    }
    
    // Statistics
    size_t getAllocCount() const { return allocCount; }
    size_t getCollectionCount() const { return collectionCount; }
    void setThreshold(size_t threshold) { gcThreshold = threshold; }
    
private:
    GarbageCollector() = default;
    
    GCObject* head = nullptr;
    std::weak_ptr<Environment> rootEnv;
    std::unordered_set<GCObject*> tempRoots;
    std::unordered_set<GCObject*> stillValid;
    bool isCollecting = false;
    
    std::atomic<size_t> allocCount{0};
    std::atomic<size_t> collectionCount{0};
    std::atomic<size_t> gcThreshold{50000};
};

// Helper to create GC-tracked string
inline Value::StringPtr makeGCString(const std::string& str) {
    return GarbageCollector::instance().track(std::make_shared<std::string>(str));
}

// Helper to create GC-tracked array
inline Value::ArrayPtr makeGCArray(const std::vector<Value>& elements = {}) {
    return GarbageCollector::instance().track(std::make_shared<EZArray>(elements));
}

#endif // GC_H
