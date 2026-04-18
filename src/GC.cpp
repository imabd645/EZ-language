#include "GC.h"
#include "Environment.h"
#include "Value.h"
#include "Interpreter.h"
#include <vector>
#include <algorithm>

// Helper to mark a Value's underlying GCObject
static void markValue(const Value& val) {
    switch (val.type()) {
        case ValueType::ARRAY: val.asArrayPtr()->gc_mark(); break;
        case ValueType::INSTANCE: val.asInstance()->gc_mark(); break;
        case ValueType::DICTIONARY: val.asDictionaryPtr()->gc_mark(); break;
        case ValueType::FUNCTION: val.asFunction()->gc_mark(); break;
        case ValueType::CLASS: val.asClass()->gc_mark(); break;
        case ValueType::BUFFER: val.asBufferPtr()->gc_mark(); break;
        case ValueType::MUTEX: val.asMutexPtr()->gc_mark(); break;
        default: break;
    }
}

void EZArray::gc_mark() {
    if (gc_marked) return;
    gc_marked = true;
    for (auto& v : elements) markValue(v);
}

void EZDictionary::gc_mark() {
    if (gc_marked) return;
    gc_marked = true;
    for (auto& pair : map) markValue(pair.second);
}

void EZInstance::gc_mark() {
    if (gc_marked) return;
    gc_marked = true;
    if (klass) klass->gc_mark();
    for (auto& pair : properties) markValue(pair.second);
}

void EZFunction::gc_mark() {
    if (gc_marked) return;
    gc_marked = true;
    if (closure) closure->gc_mark();
    if (staticEnv) staticEnv->gc_mark();
}

void EZClass::gc_mark() {
    if (gc_marked) return;
    gc_marked = true;
    if (parent) parent->gc_mark();
    for (auto& pair : methods) markValue(pair.second);
    for (auto& pair : staticMembers) markValue(pair.second);
}

void Environment::gc_mark() {
    if (gc_marked) return;
    gc_marked = true;
    if (parent) parent->gc_mark();
    for (auto& pair : variables) markValue(pair.second);
}

void GarbageCollector::registerInterpreter(Interpreter* interp) {
    std::lock_guard<std::mutex> lock(interpretersMutex);
    interpreters.push_back(interp);
}

void GarbageCollector::unregisterInterpreter(Interpreter* interp) {
    std::lock_guard<std::mutex> lock(interpretersMutex);
    auto it = std::find(interpreters.begin(), interpreters.end(), interp);
    if (it != interpreters.end()) {
        interpreters.erase(it);
    }
}

void GarbageCollector::collect(std::shared_ptr<Environment> currentEnv,
                               const std::vector<std::shared_ptr<Environment>>* envStack) {
    std::lock_guard<std::mutex> collectLock(collectMutex);
    if (isCollecting) return;
    isCollecting = true;
    collectionCount++;

    // Phase 1: Reset all marks (locked on list)
    {
        std::lock_guard<std::mutex> listLock(listMutex);
        GCObject* obj = head;
        while (obj) {
            obj->gc_marked = false;
            obj = obj->gc_next;
        }
    }

    // Phase 2: Mark from roots
    // Root 1: Global master environment (shared)
    if (auto root = rootEnv.lock()) {
        root->gc_mark();
    }

    // Root 2: Mark from ALL active interpreters (thread-safe)
    {
        std::lock_guard<std::mutex> interpLock(interpretersMutex);
        for (auto* interp : interpreters) {
            interp->gc_mark();
        }
    }

    // Root 3: Temporary roots (locked)
    {
        std::lock_guard<std::mutex> tempRootLock(tempRootsMutex);
        for (GCObject* root : tempRoots) {
            if (root && !root->gc_marked) root->gc_mark();
        }
    }

    // Phase 3: Sweep
    std::vector<GCObject*> toClear;
    {
        std::lock_guard<std::mutex> listLock(listMutex);
        GCObject* obj = head;
        while (obj) {
            if (!obj->gc_marked) {
                toClear.push_back(obj);
            }
            obj = obj->gc_next;
        }
    }

    // Clear unmarked objects
    for (GCObject* cand : toClear) {
        // We don't need to lock list for gc_clear as long as it doesn't touch head
        // However, destructors MIGHT trigger unregisterObject which locks listMutex.
        // This is safe because we are using a recursive-friendly strategy or just simple locks.
        // Wait, listMutex is NOT recursive. 
        // But unregisterObject is called during cand deletion. 
        // toClear contains pointers to objects that are about to be deleted.
        cand->gc_clear();
    }

    isCollecting = false;
}

std::shared_ptr<Environment> Environment::createChild() {
    return std::make_shared<Environment>(shared_from_this());
}
