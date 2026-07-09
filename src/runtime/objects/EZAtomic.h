#ifndef EZATOMIC_H
#define EZATOMIC_H

#include "runtime/Value.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <mutex>

struct EZAtomic {
    std::atomic<long long> val;
    EZAtomic(long long initial = 0) : val(initial) {}
    // No traverse() — holds no Value references
};

#endif // EZATOMIC_H
