#ifndef EZINTERFACE_H
#define EZINTERFACE_H

#include "runtime/Value.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <mutex>

struct EZInterface {
    std::string name;
    std::vector<std::string> requiredMethods;

    EZInterface(const std::string& name, const std::vector<std::string>& methods)
        : name(name), requiredMethods(methods) {}
    // No traverse() — holds no Value references
};


#endif // EZINTERFACE_H
