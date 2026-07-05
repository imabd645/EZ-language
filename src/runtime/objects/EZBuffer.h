#ifndef EZBUFFER_H
#define EZBUFFER_H

#include "runtime/Value.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <mutex>

struct EZBuffer {
    std::vector<uint8_t> data;
    EZBuffer(size_t size = 0) : data(size) {}
    EZBuffer(const std::vector<uint8_t>& d) : data(d) {}
    size_t size() const { return data.size(); }
    // No traverse() — holds no Value references
};


#endif // EZBUFFER_H
