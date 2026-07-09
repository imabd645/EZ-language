#ifndef EZUPVALUE_H
#define EZUPVALUE_H

#include "runtime/Value.h"
#include <atomic>

// Upvalue instances are typically owned by the VM. They are linked into a list 
// so the VM can close them when they go out of scope. 
// Closures hold pointers to them, but DO NOT own them.
struct UpvalueObj {
    std::atomic<Value*> location;   // Points to stack slot (open) or &closed (closed)
    Value  closed;                  // When closed, location == &closed
    UpvalueObj* next;
};

#endif // EZUPVALUE_H
