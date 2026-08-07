#ifndef EZINSTANCE_H
#define EZINSTANCE_H

#include "runtime/Value.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <mutex>

struct EZInstance {
    std::shared_ptr<EZClass> klass;
    std::shared_ptr<EZShape> shape;
    std::vector<Value> propertyValues;
    mutable std::shared_mutex prop_mutex;
    bool superInitialized = false;

private:
    // ── Decorator runtime state (lazily allocated) ─────────────────────────────
    std::vector<AuditEntry>*                       auditLog   = nullptr;
    std::unordered_map<std::string, CachedResult>* cacheStore = nullptr;

public:
    EZInstance(std::shared_ptr<EZClass> klass) : klass(klass), shape(klass->initialShape) {}
    ~EZInstance() {
        delete auditLog;
        delete cacheStore;
    }

    void traverse(const ValueVisitor& visit) const {
        if (klass) visit(Value(klass));
        
        std::shared_lock<std::shared_mutex> lk(prop_mutex);
        for (const auto& v : propertyValues) visit(v);
        
        if (auditLog) {
            for (const auto& e : *auditLog) {
                visit(e.oldValue);
                visit(e.newValue);
            }
        }
        if (cacheStore) {
            for (const auto& [nm, cr] : *cacheStore) visit(cr.result);
        }
    }

    Value getProperty(const std::string& name) {
        if (name == "__class__" && klass) {
            return Value(klass);
        }
        {
            std::shared_lock<std::shared_mutex> lk(prop_mutex);
            size_t offset;
            if (shape->getOffset(name, offset)) {
                return propertyValues[offset];
            }
        }
        // Search class hierarchy (class methods are set once, no lock needed)
        std::shared_ptr<EZClass> currentClass = klass;
        while (currentClass) {
            if (currentClass->hasMethod(name)) return currentClass->getMethod(name);
            currentClass = currentClass->parent;
        }
        return Value();
    }
    void setProperty(const std::string& name, const Value& value) {
        std::unique_lock<std::shared_mutex> lk(prop_mutex);
        size_t offset;
        if (shape->getOffset(name, offset)) {
            propertyValues[offset] = value;
        } else {
            shape = shape->transition(name);
            shape->getOffset(name, offset);
            if (offset >= propertyValues.size()) {
                propertyValues.resize(offset + 1);
            }
            propertyValues[offset] = value;
        }
    }
    bool hasProperty(const std::string& name) {
        std::shared_lock<std::shared_mutex> lk(prop_mutex);
        size_t offset;
        return shape->getOffset(name, offset);
    }
    std::unordered_map<std::string, Value> getPropertiesCopy() const {
        std::shared_lock<std::shared_mutex> lk(prop_mutex);
        std::unordered_map<std::string, Value> result;
        for (const auto& [name, offset] : shape->propertyOffsets) {
            result[name] = propertyValues[offset];
        }
        return result;
    }

    template<typename Func>
    void modifyProperties(Func&& func) {
        std::unique_lock<std::shared_mutex> lk(prop_mutex);
        // Warning: modifyProperties is dangerous with Shapes if they modify the map!
        // We will reconstruct the map, let them modify it, and rebuild the shape.
        std::unordered_map<std::string, Value> props;
        for (const auto& [name, offset] : shape->propertyOffsets) {
            props[name] = propertyValues[offset];
        }
        func(props);
        
        // Rebuild shape and propertyValues
        shape = std::make_shared<EZShape>();
        propertyValues.clear();
        for (const auto& [name, value] : props) {
            shape->propertyOffsets[name] = propertyValues.size();
            propertyValues.push_back(value);
        }
    }
    
    template<typename Func>
    void modifyAuditLog(Func&& func) {
        std::unique_lock<std::shared_mutex> lk(prop_mutex);
        if (!auditLog) auditLog = new std::vector<AuditEntry>();
        func(*auditLog);
    }

    template<typename Func>
    void modifyCacheStore(Func&& func) {
        std::unique_lock<std::shared_mutex> lk(prop_mutex);
        if (!cacheStore) cacheStore = new std::unordered_map<std::string, CachedResult>();
        func(*cacheStore);
    }

    std::vector<AuditEntry>* getAuditLog() { return auditLog; }
    std::unordered_map<std::string, CachedResult>* getCacheStore() { return cacheStore; }

    std::vector<AuditEntry> getAuditLogCopy() const {
        std::shared_lock<std::shared_mutex> lk(prop_mutex);
        if (!auditLog) return {};
        return *auditLog;
    }
};


#endif // EZINSTANCE_H
