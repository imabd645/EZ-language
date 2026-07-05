#ifndef EZARRAY_H
#define EZARRAY_H

#include "runtime/Value.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <mutex>

struct EZArray {
    std::vector<Value> elements;
    EZArray(const std::vector<Value>& e = {}) : elements(e) {}

    void traverse(const ValueVisitor& visit) const {
        for (const Value& v : elements) visit(v);
    }

    size_t size() const { return elements.size(); }
    bool empty() const { return elements.empty(); }
    void push_back(const Value& v) { elements.push_back(v); }
    void pop_back() { elements.pop_back(); }
    void resize(size_t newSize) { elements.resize(newSize); }
    Value& back() { return elements.back(); }
    Value& operator[](size_t i) { return elements[i]; }
    const Value& operator[](size_t i) const { return elements[i]; }
    auto begin() { return elements.begin(); }
    auto end() { return elements.end(); }
    auto begin() const { return elements.begin(); }
    auto end() const { return elements.end(); }
    void erase(std::vector<Value>::iterator it) { elements.erase(it); }
    void insert(std::vector<Value>::iterator it, const Value& v) { elements.insert(it, v); }
};


#endif // EZARRAY_H
