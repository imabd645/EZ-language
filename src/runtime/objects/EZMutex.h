#ifndef EZMUTEX_H
#define EZMUTEX_H

#include "runtime/Value.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <mutex>

struct EZMutex {
    std::recursive_mutex mtx;
    void lock()   { mtx.lock(); }
    void unlock() { mtx.unlock(); }
    // No traverse() — holds no Value references
};


#endif // EZMUTEX_H
