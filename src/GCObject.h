#ifndef GCOBJECT_H
#define GCOBJECT_H

#include <memory>

class GCObject {
public:
    bool gc_marked = false;
    GCObject* gc_next = nullptr;
    GCObject* gc_prev = nullptr;

    GCObject();
    virtual ~GCObject();

    virtual void gc_mark() = 0;
    virtual void gc_clear() = 0;
};

#endif // GCOBJECT_H
