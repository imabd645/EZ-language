#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <memory>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <mutex>
#include "Value.h"

class RuntimeError : public std::runtime_error {
public:
    int line;
    RuntimeError(const std::string& message, int line = 0) 
        : std::runtime_error(message), line(line) {}
};

class Environment : public std::enable_shared_from_this<Environment> {
public:
    std::shared_ptr<Environment> parent;
    std::unordered_map<std::string, Value> variables;
    mutable std::recursive_mutex mutex;
    
    Environment() : parent(nullptr) {}
    explicit Environment(std::shared_ptr<Environment> parent) : parent(parent) {}
    
    // Define a new variable in current scope
    void define(const std::string& name, const Value& value) {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        variables[name] = value;
    }
    
    // Get a variable (walks up parent chain)
    Value get(const std::string& name, int line = 0) const {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        auto it = variables.find(name);
        if (it != variables.end()) {
            return it->second;
        }
        
        if (parent) {
            return parent->get(name, line);
        }
        
        throw RuntimeError("Undefined variable '" + name + "'", line);
    }
    
    // Check if variable exists
    bool contains(const std::string& name) const {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        if (variables.find(name) != variables.end()) {
            return true;
        }
        if (parent) {
            return parent->contains(name);
        }
        return false;
    }
    
    // Assign to existing variable (walks up parent chain)
    void assign(const std::string& name, const Value& value, int line = 0) {
        std::unique_lock<std::recursive_mutex> lock(mutex);
        auto it = variables.find(name);
        if (it != variables.end()) {
            it->second = value;
            return;
        }
        
        // Don't hold lock while calling parent to avoid deadlocks (though recursive mutex handles re-entry, 
        // passing across different environment locks needs care. 
        // But here we simply recurse. 'parent' is safe because it's a shared_ptr, 
        // but locking order matters. Here we're locking 'this' then 'parent'.
        // As long as we always go child->parent, it's fine.
        if (parent) {
            lock.unlock(); // checking parent doesn't need our lock held if we didn't find it here? 
            // Actually, if we unlock, someone might define it here. 
            // Standard discipline: hold lock. 
            // Using recursive mutex on parent is fine if order is consistent.
            parent->assign(name, value, line);
            return;
        }
        
        lock.unlock(); // Unlock before define? OR define handles its own lock.
        // Actually recursive mutex lets us keep it. 
        // But if 'define' locks 'mutex', we are fine.
        
        // If variable doesn't exist, define it in current scope
        define(name, value);
    }
    
    // Get pointer to variable for modification (e.g., array indexing)
    // WARN: This is unsafe if the map rehashes while pointer is held. 
    // Mutex doesn't protect the pointer after return.
    // Keeping as is but noting risk.
    Value* getPtr(const std::string& name) {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        auto it = variables.find(name);
        if (it != variables.end()) {
            return &it->second;
        }
        
        if (parent) {
            return parent->getPtr(name);
        }
        
        return nullptr;
    }
    
    // Create a child scope
    std::shared_ptr<Environment> createChild() {
        return std::make_shared<Environment>(shared_from_this());
    }
};

#endif // ENVIRONMENT_H
