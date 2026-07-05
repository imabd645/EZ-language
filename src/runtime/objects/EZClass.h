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
    std::unordered_map<std::string, Value> methods;
    std::unordered_map<std::string, Value> staticMembers;
    std::unordered_map<std::string, bool>  visibility;

    // Legacy support for AST Interpreter
    std::vector<std::string> initParams;
    std::vector<StmtPtr> initBody;

    // ── Decorator metadata ────────────────────────────────────────────────────
    BehaviorFlags behaviors = {false,false,false,false,false};
    std::string persistPath;
    std::vector<FieldValidator> validators;
    std::unordered_set<std::string> cachedMethods;

    EZClass(const std::string& name) : name(name), parent(nullptr) {}

    void traverse(const ValueVisitor& visit) const {
        if (parent) visit(Value(parent));
        for (const auto& [k, v] : methods)       visit(v);
        for (const auto& [k, v] : staticMembers) visit(v);
        for (const auto& fv : validators)        visit(fv.param);
    }
};


#endif // EZCLASS_H
