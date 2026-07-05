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
    std::unordered_map<std::string, Value> properties;
    mutable std::shared_mutex prop_mutex;

    // ── Decorator runtime state (lazily allocated) ─────────────────────────────
    std::vector<AuditEntry>*                       auditLog   = nullptr;
    std::unordered_map<std::string, CachedResult>* cacheStore = nullptr;

    EZInstance(std::shared_ptr<EZClass> klass) : klass(klass) {}
    ~EZInstance() {
        delete auditLog;
        delete cacheStore;
    }

    void traverse(const ValueVisitor& visit) const {
        if (klass) visit(Value(klass));
        {
            std::shared_lock<std::shared_mutex> lk(prop_mutex);
            for (const auto& [k, v] : properties) visit(v);
        }
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
            auto it = properties.find(name);
            if (it != properties.end()) return it->second;
        }
        // Search class hierarchy (class methods are set once, no lock needed)
        std::shared_ptr<EZClass> currentClass = klass;
        while (currentClass) {
            if (currentClass->methods.count(name)) return currentClass->methods[name];
            currentClass = currentClass->parent;
        }
        return Value();
    }
    void setProperty(const std::string& name, const Value& value) {
        std::unique_lock<std::shared_mutex> lk(prop_mutex);
        properties[name] = value;
    }
    bool hasProperty(const std::string& name) {
        std::shared_lock<std::shared_mutex> lk(prop_mutex);
        return properties.count(name) > 0;
    }
};


#endif // EZINSTANCE_H
