#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <memory>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <shared_mutex>
#include "Value.h"
#include "GCObject.h"

class RuntimeError : public std::runtime_error {
public:
    int line;
    Value value;
    RuntimeError(const std::string& message, int line = 0, Value val = Value()) 
        : std::runtime_error(message), line(line), value(val) {}
};

class Environment : public GCObject, public std::enable_shared_from_this<Environment> {
public:
    std::shared_ptr<Environment> parent;
    std::unordered_map<std::string, Value> variables;
    mutable std::shared_mutex mutex;
    bool isStatic = false;

    void gc_mark() override;
    void gc_clear() override { variables.clear(); parent = nullptr; }
    
    Environment() : parent(nullptr) {}
    explicit Environment(std::shared_ptr<Environment> parent, bool isStatic = false) 
        : parent(parent), isStatic(isStatic) {}
    
    // Define a new variable in current scope
    void define(const std::string& name, const Value& value) {
        std::unique_lock<std::shared_mutex> lock(mutex);
        variables[name] = value;
    }
    
    // Get a variable (walks up parent chain)
    Value get(const std::string& name, int line = 0) const {
        std::shared_lock<std::shared_mutex> lock(mutex);
        auto it = variables.find(name);
        if (it != variables.end()) {
            return it->second;
        }
        
        if (parent) {
            return parent->get(name, line);
        }
        
        return Value(); // Return nil, let Interpreter handle undefined error
    }
    
    // Check if variable exists
    bool contains(const std::string& name) const {
        std::shared_lock<std::shared_mutex> lock(mutex);
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
        {
            std::unique_lock<std::shared_mutex> lock(mutex);
            auto it = variables.find(name);
            if (it != variables.end()) {
                it->second = value;
                return;
            }
        } // Release lock before recursing or defining

        if (parent) {
            parent->assign(name, value, line);
            return;
        }

        define(name, value); // define() handles its own lock
    }
    
    // Get pointer to variable for modification (e.g., array indexing)
    // WARN: This is unsafe if the map rehashes while pointer is held. 
    // Mutex doesn't protect the pointer after return.
    // Keeping as is but noting risk.
    // Get pointer to variable for modification (e.g., array indexing)
    // WARN: This is unsafe if the map rehashes while pointer is held. 
    // Mutex doesn't protect the pointer after return.
    // Keeping as is but noting risk.
    Value* getPtr(const std::string& name) {
        std::shared_lock<std::shared_mutex> lock(mutex);
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
    std::shared_ptr<Environment> createChild();

};

#endif // ENVIRONMENT_H
