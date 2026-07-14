#ifndef EZSHAPE_H
#define EZSHAPE_H

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <shared_mutex>

struct EZShape {
    std::unordered_map<std::string, size_t> propertyOffsets;
    std::unordered_map<std::string, std::shared_ptr<EZShape>> transitions;
    mutable std::shared_mutex transition_mutex;

    EZShape() = default;

    // Fast path: property already exists in this shape
    bool getOffset(const std::string& name, size_t& outOffset) const {
        std::shared_lock<std::shared_mutex> lk(transition_mutex);
        auto it = propertyOffsets.find(name);
        if (it != propertyOffsets.end()) {
            outOffset = it->second;
            return true;
        }
        return false;
    }

    // Slow path: transitioning to a new shape by adding a property
    std::shared_ptr<EZShape> transition(const std::string& name) {
        {
            std::shared_lock<std::shared_mutex> lk(transition_mutex);
            auto it = transitions.find(name);
            if (it != transitions.end()) {
                return it->second;
            }
        }
        
        // Need to create a new shape
        std::unique_lock<std::shared_mutex> lk(transition_mutex);
        // Double check
        auto it = transitions.find(name);
        if (it != transitions.end()) {
            return it->second;
        }

        auto newShape = std::make_shared<EZShape>();
        // Copy existing offsets
        newShape->propertyOffsets = this->propertyOffsets;
        // Add new offset
        newShape->propertyOffsets[name] = this->propertyOffsets.size();
        
        transitions[name] = newShape;
        return newShape;
    }
};

#endif // EZSHAPE_H
