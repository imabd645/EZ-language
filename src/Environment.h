#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <memory>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <shared_mutex>
#include "Value.h"

class RuntimeError : public std::runtime_error {
public:
    int line;
    Value value;
    RuntimeError(const std::string& message, int line = 0, Value val = Value())
        : std::runtime_error(message), line(line), value(val) {}
};

class Environment : public std::enable_shared_from_this<Environment> {
public:
    std::shared_ptr<Environment> parent;
    std::unordered_map<std::string, Value> variables;
    mutable std::shared_mutex mutex;
    bool isStatic = false;

    Environment() : parent(nullptr), version(0) {}
    explicit Environment(std::shared_ptr<Environment> parent, bool isStatic = false)
        : parent(parent), isStatic(isStatic), version(0) {}

    uint64_t version = 0;

    // traverse() — called by CycleCollector to enumerate outgoing Value refs
    void traverse(const ValueVisitor& visit) const {
        std::shared_lock<std::shared_mutex> lock(mutex);
        for (const auto& [k, v] : variables) visit(v);
        // Note: 'parent' is a shared_ptr<Environment>, not a Value —
        // the cycle collector tracks Environment separately if needed.
        // Environment cycles (scope chains) are broken naturally when
        // the VM unwinds frames. No Environment objects are tracked by
        // CycleCollector — only Value containers are.
    }

    // Define a new variable in current scope
    void define(const std::string& name, const Value& value) {
        std::unique_lock<std::shared_mutex> lock(mutex);
        variables[name] = value;
        version++;
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

    // Create a child scope
    std::shared_ptr<Environment> createChild();
};

#endif // ENVIRONMENT_H
