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
    mutable std::shared_mutex array_mutex;

private:
    std::vector<Value> elements;

public:
    EZArray(const std::vector<Value>& e = {}) : elements(e) {}

    void traverse(const ValueVisitor& visit) const {
        std::shared_lock<std::shared_mutex> lk(array_mutex);
        for (const Value& v : elements) visit(v);
    }

    size_t size() const { 
        std::shared_lock<std::shared_mutex> lk(array_mutex);
        return elements.size(); 
    }
    bool empty() const { 
        std::shared_lock<std::shared_mutex> lk(array_mutex);
        return elements.empty(); 
    }
    void push_back(const Value& v) { 
        std::unique_lock<std::shared_mutex> lk(array_mutex);
        elements.push_back(v); 
    }
    void pop_back() { 
        std::unique_lock<std::shared_mutex> lk(array_mutex);
        if(!elements.empty()) elements.pop_back(); 
    }
    void resize(size_t newSize) { 
        std::unique_lock<std::shared_mutex> lk(array_mutex);
        elements.resize(newSize); 
    }
    Value back() const { 
        std::shared_lock<std::shared_mutex> lk(array_mutex);
        return elements.empty() ? Value() : elements.back(); 
    }
    Value operator[](size_t i) const { 
        std::shared_lock<std::shared_mutex> lk(array_mutex);
        if (i >= elements.size()) return Value();
        return elements[i]; 
    }
    void set(size_t i, const Value& v) {
        std::unique_lock<std::shared_mutex> lk(array_mutex);
        if (i < elements.size()) elements[i] = v;
    }
    void erase(size_t index) { 
        std::unique_lock<std::shared_mutex> lk(array_mutex);
        if(index < elements.size()) elements.erase(elements.begin() + index); 
    }
    void insert(size_t index, const Value& v) { 
        std::unique_lock<std::shared_mutex> lk(array_mutex);
        if(index <= elements.size()) elements.insert(elements.begin() + index, v); 
    }

    std::vector<Value> getElementsCopy() const {
        std::shared_lock<std::shared_mutex> lk(array_mutex);
        return elements;
    }
    
    template<typename Func>
    void modifyElements(Func&& func) {
        std::unique_lock<std::shared_mutex> lk(array_mutex);
        func(elements);
    }
};


#endif // EZARRAY_H
