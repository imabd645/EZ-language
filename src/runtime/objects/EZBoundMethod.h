#ifndef EZBOUNDMETHOD_H
#define EZBOUNDMETHOD_H

#include "runtime/Value.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <mutex>

struct EZBoundMethod {
    Value receiver;
    Value method;
    EZBoundMethod(const Value& receiver, const Value& method)
        : receiver(receiver), method(method) {}

    void traverse(const ValueVisitor& visit) const {
        visit(receiver);
        visit(method);
    }
};

#include <atomic>

struct UpvalueObj {
    std::atomic<Value*> location;   // Points to stack slot (open) or &closed (closed)
    Value  closed;     // When closed, location == &closed
    UpvalueObj* next;
};


#endif // EZBOUNDMETHOD_H
