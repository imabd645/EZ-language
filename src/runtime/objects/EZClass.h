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

    // ── Attribute-hook presence, cached ──────────────────────────────────────
    //
    // __setattr__ is consulted on EVERY property write. Resolving it through the
    // methods map each time costs a shared_lock plus a hash lookup per store --
    // paid by every class, while essentially none define the hook. These flags
    // reduce that to a single bool test on the hot path; the map is only
    // consulted once a flag says there is something to find.
    //
    // Recomputed whenever the method tables change, and at the end of class
    // construction (which is also when a parent's methods have been flattened
    // in, so an inherited hook is already visible here).
    bool hasGetattrHook = false;
    bool hasSetattrHook = false;

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
        {
            std::unique_lock<std::shared_mutex> lk(class_mutex);
            methods[name] = val;
        }
        if (name == "__getattr__") hasGetattrHook = true;
        if (name == "__setattr__") hasSetattrHook = true;
    }

    void setStaticMember(const std::string& name, const Value& val) {
        {
            std::unique_lock<std::shared_mutex> lk(class_mutex);
            staticMembers[name] = val;
        }
        if (name == "__getattr__") hasGetattrHook = true;
    }

    // Recompute the cached hook flags from the current tables. Call after
    // populating a class directly (the VM builds the maps in place rather than
    // going through setMethod for every member).
    void refreshAttrHookFlags() {
        bool g = false, s = false;
        {
            std::shared_lock<std::shared_mutex> lk(class_mutex);
            g = methods.count("__getattr__") > 0 || staticMembers.count("__getattr__") > 0;
            s = methods.count("__setattr__") > 0;
        }
        // Statics are not flattened into a subclass the way methods are, so an
        // inherited static __getattr__ has to be picked up from the parent.
        if (parent) {
            g = g || parent->hasGetattrHook;
            s = s || parent->hasSetattrHook;
        }
        hasGetattrHook = g;
        hasSetattrHook = s;
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
