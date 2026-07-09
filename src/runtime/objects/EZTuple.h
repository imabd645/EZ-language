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
    mutable std::shared_mutex tuple_mutex;

private:
    std::vector<Value> elements;

public:
    EZTuple(const std::vector<Value>& e = {}) : elements(e) {}

    void traverse(const ValueVisitor& visit) const {
        std::shared_lock<std::shared_mutex> lk(tuple_mutex);
        for (const Value& v : elements) visit(v);
    }

    template<typename Func>
    void modifyElements(Func&& func) {
        std::unique_lock<std::shared_mutex> lk(tuple_mutex);
        func(elements);
    }

    size_t size() const { 
        std::shared_lock<std::shared_mutex> lk(tuple_mutex);
        return elements.size(); 
    }
    bool empty() const { 
        std::shared_lock<std::shared_mutex> lk(tuple_mutex);
        return elements.empty(); 
    }
    Value operator[](size_t i) const { 
        std::shared_lock<std::shared_mutex> lk(tuple_mutex);
        if (i >= elements.size()) return Value();
        return elements[i]; 
    }
    
    std::vector<Value> getElementsCopy() const {
        std::shared_lock<std::shared_mutex> lk(tuple_mutex);
        return elements;
    }
};


#endif // EZTUPLE_H
