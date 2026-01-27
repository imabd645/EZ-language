#ifndef GC_H
#define GC_H

#include <vector>
#include <unordered_set>
#include <memory>
#include <atomic>
#include "Value.h"

class Environment;

// Simple mark-and-sweep garbage collector
class GarbageCollector {
public:
    static GarbageCollector& instance() {
        static GarbageCollector gc;
        return gc;
    }
    
    // Track an allocation
    template<typename T>
    std::shared_ptr<T> track(std::shared_ptr<T> ptr) {
        // In this implementation, we rely on shared_ptr reference counting
        // which provides automatic memory management. For a more advanced
        // GC, we would track weak references and do periodic collection.
        allocCount++;
        
        if (allocCount > gcThreshold) {
            // Trigger collection
            collect();
            allocCount = 0;
        }
        
        return ptr;
    }
    
    // Set root environment for marking
    void setRoot(std::shared_ptr<Environment> root) {
        rootEnv = root;
    }
    
    // Manual collection trigger
    void collect() {
        // With shared_ptr, collection happens automatically through RAII
        // This is a placeholder for more advanced GC strategies
        collectionCount++;
    }
    
    // Statistics
    size_t getAllocCount() const { return allocCount; }
    size_t getCollectionCount() const { return collectionCount; }
    
    // Configure threshold
    void setThreshold(size_t threshold) { gcThreshold = threshold; }
    
private:
    GarbageCollector() = default;
    
    std::weak_ptr<Environment> rootEnv;
    std::atomic<size_t> allocCount{0};
    std::atomic<size_t> collectionCount{0};
    std::atomic<size_t> gcThreshold{1000}; // Collect every 1000 allocations
};

// Helper to create GC-tracked string
inline Value::StringPtr makeGCString(const std::string& str) {
    return GarbageCollector::instance().track(std::make_shared<std::string>(str));
}

// Helper to create GC-tracked array
inline Value::ArrayPtr makeGCArray(const std::vector<Value>& elements = {}) {
    return GarbageCollector::instance().track(std::make_shared<Value::ArrayType>(elements));
}

#endif // GC_H
