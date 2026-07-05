#ifndef EZTUPLE_H
#define EZTUPLE_H

#include "runtime/Value.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <mutex>

struct EZTuple {
    std::vector<Value> elements;
    EZTuple(const std::vector<Value>& e = {}) : elements(e) {}

    void traverse(const ValueVisitor& visit) const {
        for (const Value& v : elements) visit(v);
    }

    size_t size() const { return elements.size(); }
    bool empty() const { return elements.empty(); }
    const Value& operator[](size_t i) const { return elements[i]; }
    auto begin() const { return elements.begin(); }
    auto end() const { return elements.end(); }
};


#endif // EZTUPLE_H
