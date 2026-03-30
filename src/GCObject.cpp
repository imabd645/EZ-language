#include "GCObject.h"
#include "GC.h"

GCObject::GCObject() {
    GarbageCollector::instance().registerObject(this);
    GarbageCollector::instance().incrementAllocCount();
}

GCObject::~GCObject() {
    GarbageCollector::instance().unregisterObject(this);
}
