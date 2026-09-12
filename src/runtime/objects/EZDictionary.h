#ifndef EZDICTIONARY_H
#define EZDICTIONARY_H

#include "runtime/Value.h"
#include "RecursiveTeardown.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <mutex>

struct EZDictionary {
    mutable std::shared_mutex map_mutex;

private:
    std::unordered_map<std::string, Value> map;

public:
    // See RecursiveTeardown.h / EZArray's matching destructor -- a long
    // singly-owned chain of nested dictionaries has the same recursive-
    // destructor stack-overflow risk as nested arrays or a linked list of
    // instances.
    ~EZDictionary() {
        std::vector<Value*> children;
        children.reserve(map.size());
        for (auto& kv : map) children.push_back(&kv.second);
        ezIterativelyReleaseChildren(std::move(children));
    }

    // Used by RecursiveTeardownImpl.h's teardown helper, which is a free
    // function and so can't reach the private `map` member directly.
    void appendChildPointers(std::vector<Value*>& out) {
        for (auto& kv : map) out.push_back(&kv.second);
    }

    void traverse(const ValueVisitor& visit) const {
        std::shared_lock<std::shared_mutex> lk(map_mutex);
        for (const auto& [k, v] : map) visit(v);
    }

    size_t size() const {
        std::shared_lock<std::shared_mutex> lk(map_mutex);
        return map.size();
    }
    
    bool empty() const {
        std::shared_lock<std::shared_mutex> lk(map_mutex);
        return map.empty();
    }
    
    bool has(const std::string& key) const {
        std::shared_lock<std::shared_mutex> lk(map_mutex);
        return map.count(key) > 0;
    }
    
    Value get(const std::string& key) const {
        std::shared_lock<std::shared_mutex> lk(map_mutex);
        auto it = map.find(key);
        if (it != map.end()) return it->second;
        return Value();
    }
    
    void set(const std::string& key, const Value& value) {
        std::unique_lock<std::shared_mutex> lk(map_mutex);
        map[key] = value;
    }
    
    void erase(const std::string& key) {
        std::unique_lock<std::shared_mutex> lk(map_mutex);
        map.erase(key);
    }

    std::unordered_map<std::string, Value> getMapCopy() const {
        std::shared_lock<std::shared_mutex> lk(map_mutex);
        return map;
    }
    
    template<typename Func>
    void modifyMap(Func&& func) {
        std::unique_lock<std::shared_mutex> lk(map_mutex);
        func(map);
    }
    
    template<typename Func>
    void readMap(Func&& func) const {
        std::shared_lock<std::shared_mutex> lk(map_mutex);
        func(map);
    }
};


#endif // EZDICTIONARY_H
