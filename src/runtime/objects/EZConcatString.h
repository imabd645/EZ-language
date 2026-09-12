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

    ~EZConcatString() {
        std::vector<Value> todo;
        if (!left.isNil()) todo.push_back(std::move(left));
        if (!right.isNil()) todo.push_back(std::move(right));

        while (!todo.empty()) {
            Value v = std::move(todo.back());
            todo.pop_back();

            if (v.type() == ValueType::CONCAT_STRING) {
                auto ptr = v.asConcatStringPtr();
                // ptr and v both hold a reference, so if it's uniquely owned by us, use_count will be 2
                if (ptr.use_count() == 2) {
                    if (!ptr->left.isNil()) todo.push_back(std::move(ptr->left));
                    if (!ptr->right.isNil()) todo.push_back(std::move(ptr->right));
                    // Clear the children to prevent recursion when ptr is destroyed
                    ptr->left = Value();
                    ptr->right = Value();
                }
            }
        }
    }
};

// --- Container structs (owned by shared_ptr, tracked by CycleCollector) ---


#endif // EZCONCATSTRING_H
