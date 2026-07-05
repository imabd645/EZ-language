#ifndef EZCLOSURE_H
#define EZCLOSURE_H

#include "runtime/Value.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <mutex>

struct EZClosure {
    std::shared_ptr<struct BytecodeFunction> function;
    std::vector<UpvalueObj*> upvalues;
    EZClosure(std::shared_ptr<struct BytecodeFunction> f) : function(f) {}

    void traverse(const ValueVisitor& visit) const {
        for (UpvalueObj* uv : upvalues) {
            if (uv) visit(uv->closed);
        }
    }
};


#endif // EZCLOSURE_H
