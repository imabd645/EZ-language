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
    mutable std::shared_mutex prop_mutex;

private:
    std::unordered_map<std::string, Value> properties;

    // ── Decorator runtime state (lazily allocated) ─────────────────────────────
    std::vector<AuditEntry>*                       auditLog   = nullptr;
    std::unordered_map<std::string, CachedResult>* cacheStore = nullptr;

public:
    EZInstance(std::shared_ptr<EZClass> klass) : klass(klass) {}
    ~EZInstance() {
        delete auditLog;
        delete cacheStore;
    }

    void traverse(const ValueVisitor& visit) const {
        if (klass) visit(Value(klass));
        
        std::shared_lock<std::shared_mutex> lk(prop_mutex);
        for (const auto& [k, v] : properties) visit(v);
        
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
            if (currentClass->hasMethod(name)) return currentClass->getMethod(name);
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
    std::unordered_map<std::string, Value> getPropertiesCopy() const {
        std::shared_lock<std::shared_mutex> lk(prop_mutex);
        return properties;
    }

    template<typename Func>
    void modifyProperties(Func&& func) {
        std::unique_lock<std::shared_mutex> lk(prop_mutex);
        func(properties);
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
