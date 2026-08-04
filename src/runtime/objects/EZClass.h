#ifndef EZCLASS_H
#define EZCLASS_H

#include "runtime/Value.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <mutex>

struct EZClass {
    std::string name;
    std::shared_ptr<EZClass> parent;
    std::shared_ptr<struct EZShape> initialShape;
    mutable std::shared_mutex class_mutex;

    std::unordered_map<std::string, Value> methods;
    std::unordered_map<std::string, Value> staticMembers;
    std::unordered_map<std::string, bool>  visibility;

    // ── Decorator metadata ────────────────────────────────────────────────────
    BehaviorFlags behaviors = {false,false,false,false,false};
    std::string persistPath;
    std::vector<FieldValidator> validators;
    std::unordered_set<std::string> cachedMethods;

    EZClass(const std::string& name);
    void traverse(const ValueVisitor& visit) const {
        if (parent) visit(Value(parent));
        
        std::shared_lock<std::shared_mutex> lk(class_mutex);
        for (const auto& [k, v] : methods)       visit(v);
        for (const auto& [k, v] : staticMembers) visit(v);
        for (const auto& fv : validators)        visit(fv.param);
    }

    Value getMethod(const std::string& name) const {
        std::shared_lock<std::shared_mutex> lk(class_mutex);
        auto it = methods.find(name);
        if (it != methods.end()) return it->second;
        return Value();
    }
    
    Value getStaticMember(const std::string& name) const {
        std::shared_lock<std::shared_mutex> lk(class_mutex);
        auto it = staticMembers.find(name);
        if (it != staticMembers.end()) return it->second;
        return Value();
    }
    
    void setMethod(const std::string& name, const Value& val) {
        std::unique_lock<std::shared_mutex> lk(class_mutex);
        methods[name] = val;
    }

    void setStaticMember(const std::string& name, const Value& val) {
        std::unique_lock<std::shared_mutex> lk(class_mutex);
        staticMembers[name] = val;
    }

    bool hasMethod(const std::string& name) const {
        std::shared_lock<std::shared_mutex> lk(class_mutex);
        return methods.count(name) > 0;
    }

    // Method lookup that walks the inheritance chain, unlike getMethod() which
    // only looks at this class. Used for the __getattr__/__setattr__ hooks: a
    // base class defining one must serve every subclass, or the hook would have
    // to be repeated in each of them.
    Value findMethod(const std::string& name) const {
        const EZClass* c = this;
        while (c) {
            Value v = c->getMethod(name);
            if (!v.isNil()) return v;
            c = c->parent.get();
        }
        return Value();
    }

    // Same, for statics.
    Value findStaticMember(const std::string& name) const {
        const EZClass* c = this;
        while (c) {
            Value v = c->getStaticMember(name);
            if (!v.isNil()) return v;
            c = c->parent.get();
        }
        return Value();
    }

    bool hasStaticMember(const std::string& name) const {
        std::shared_lock<std::shared_mutex> lk(class_mutex);
        return staticMembers.count(name) > 0;
    }
    
    template<typename Func>
    void modifyMethods(Func&& func) {
        std::unique_lock<std::shared_mutex> lk(class_mutex);
        func(methods);
    }
    
    template<typename Func>
    void modifyStaticMembers(Func&& func) {
        std::unique_lock<std::shared_mutex> lk(class_mutex);
        func(staticMembers);
    }
};


#endif // EZCLASS_H
