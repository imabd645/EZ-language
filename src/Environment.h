#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <memory>
#include <string>
#include <unordered_map>
#include <stdexcept>
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
    
    Environment() : parent(nullptr) {}
    explicit Environment(std::shared_ptr<Environment> parent) : parent(parent) {}
    
    // Define a new variable in current scope
    void define(const std::string& name, const Value& value) {
        variables[name] = value;
    }
    
    // Get a variable (walks up parent chain)
    Value get(const std::string& name, int line = 0) const {
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
        auto it = variables.find(name);
        if (it != variables.end()) {
            it->second = value;
            return;
        }
        
        if (parent) {
            parent->assign(name, value, line);
            return;
        }
        
        // If variable doesn't exist, define it in current scope
        define(name, value);
    }
    
    // Get pointer to variable for modification (e.g., array indexing)
    Value* getPtr(const std::string& name) {
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
