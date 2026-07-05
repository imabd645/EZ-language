#ifndef EZDICTIONARY_H
#define EZDICTIONARY_H

#include "runtime/Value.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <mutex>

struct EZDictionary {
    std::unordered_map<std::string, Value> map;
    mutable std::shared_mutex map_mutex;

    void traverse(const ValueVisitor& visit) const {
        std::shared_lock<std::shared_mutex> lk(map_mutex);
        for (const auto& [k, v] : map) visit(v);
    }
};


#endif // EZDICTIONARY_H
