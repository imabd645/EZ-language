#ifndef EZCONCATSTRING_H
#define EZCONCATSTRING_H

#include "runtime/Value.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <mutex>

struct EZConcatString {
    Value left;
    Value right;
    size_t length = 0;
    bool isFlattened = false;
    std::shared_ptr<std::string> flattened;

    void traverse(const ValueVisitor& visit) const {
        visit(left);
        visit(right);
    }
};

// --- Container structs (owned by shared_ptr, tracked by CycleCollector) ---


#endif // EZCONCATSTRING_H
