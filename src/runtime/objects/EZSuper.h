#ifndef EZSUPER_H
#define EZSUPER_H

#include "runtime/Value.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <mutex>

struct EZSuper {
    std::shared_ptr<EZInstance> instance;
    std::shared_ptr<EZClass> parentKlass;
    EZSuper(std::shared_ptr<EZInstance> instance, std::shared_ptr<EZClass> parentKlass) 
        : instance(instance), parentKlass(parentKlass) {}
};


#endif // EZSUPER_H
