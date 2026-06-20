#include "GC.h"
#include "Environment.h"
#include "Value.h"
#include <vector>
#include <shared_mutex>

// Helper to mark a Value's underlying GCObject
void GarbageCollector::markValue(const Value& val) {
    switch (val.type()) {
        case ValueType::ARRAY: val.asArrayPtr()->gc_mark(); break;
        case ValueType::INSTANCE: val.asInstance()->gc_mark(); break;
        case ValueType::DICTIONARY: val.asDictionaryPtr()->gc_mark(); break;
        case ValueType::FUNCTION: val.asFunction()->gc_mark(); break;
        case ValueType::CLASS: val.asClass()->gc_mark(); break;
        case ValueType::BUFFER: val.asBufferPtr()->gc_mark(); break;
        case ValueType::MUTEX: val.asMutexPtr()->gc_mark(); break;
        case ValueType::BOUND_METHOD: val.asBoundMethod()->gc_mark(); break;
        default: break;
    }
}

void EZArray::gc_mark() {
    if (gc_marked) return;
    gc_marked = true;
    for (auto& v : elements) GarbageCollector::markValue(v);
}

void EZDictionary::gc_mark() {
    if (gc_marked) return;
    gc_marked = true;
    std::shared_lock<std::shared_mutex> lk(map_mutex);
    for (auto& pair : map) GarbageCollector::markValue(pair.second);
}

void EZInstance::gc_mark() {
    if (gc_marked) return;
    gc_marked = true;
    if (klass) klass->gc_mark();
    {
        std::shared_lock<std::shared_mutex> lk(prop_mutex);
        for (auto& pair : properties) GarbageCollector::markValue(pair.second);
    }
    // Mark audit log values
    if (auditLog) {
        for (auto& e : *auditLog) {
            GarbageCollector::markValue(e.oldValue);
            GarbageCollector::markValue(e.newValue);
        }
    }
    // Mark cached results
    if (cacheStore) {
        for (auto& [name, cr] : *cacheStore) {
            GarbageCollector::markValue(cr.result);
        }
    }
}

void EZFunction::gc_mark() {
    if (gc_marked) return;
    gc_marked = true;
    if (closure) closure->gc_mark();
    if (staticEnv) staticEnv->gc_mark();
}

void EZClosure::gc_mark() {
    if (gc_marked) return;
    gc_marked = true;
    for (auto uv : upvalues) {
        if (uv) GarbageCollector::markValue(uv->closed);
    }
}

void EZClosure::gc_clear() {
    upvalues.clear();
    function = nullptr;
}

void EZBoundMethod::gc_mark() {
    if (gc_marked) return;
    gc_marked = true;
    GarbageCollector::markValue(receiver);
    GarbageCollector::markValue(method);
}

void EZBoundMethod::gc_clear() {
    receiver = Value();
    method = Value();
}

void EZClass::gc_mark() {
    if (gc_marked) return;
    gc_marked = true;
    if (parent) parent->gc_mark();
    for (auto& pair : methods) GarbageCollector::markValue(pair.second);
    for (auto& pair : staticMembers) GarbageCollector::markValue(pair.second);
    // Mark validator param values
    for (auto& v : validators) GarbageCollector::markValue(v.param);
}

void Environment::gc_mark() {
    if (gc_marked) return;
    gc_marked = true;
    if (parent) parent->gc_mark();
    for (auto& pair : variables) GarbageCollector::markValue(pair.second);
}

thread_local int tls_vm_depth = 0;

VMScope::VMScope() {
    if (tls_vm_depth++ == 0) GarbageCollector::instance().enterVMExecution();
}
VMScope::~VMScope() {
    if (--tls_vm_depth == 0) GarbageCollector::instance().leaveVMExecution();
}

void GarbageCollector::collect(std::shared_ptr<Environment> currentEnv,
                               const std::vector<std::shared_ptr<Environment>>* envStack,
                               bool callerIsVM) {
    std::lock_guard<std::recursive_mutex> lock(gc_mutex);
    if (isCollecting) return;
    isCollecting = true;
    collectionCount++;

    // Stop-The-World
    gc_requested.store(true, std::memory_order_release);
    {
        std::unique_lock<std::mutex> stw_lock(stw_mutex);
        stw_cv.wait(stw_lock, [this, callerIsVM]() {
            return active_vm_threads.load(std::memory_order_acquire) <= (callerIsVM ? 1 : 0);
        });
    }

    // Phase 1: Reset all marks
    GCObject* obj = head;
    while (obj) {
        obj->gc_marked = false;
        obj = obj->gc_next;
    }

    // Phase 2: Mark from roots
    // Root 1: Global environment
    if (auto root = rootEnv.lock()) {
        root->gc_mark();
    }

    // Root 2: Current execution environment
    if (currentEnv) {
        currentEnv->gc_mark();
    }

    // Root 3: All saved environments on the interpreter's call stack
    if (envStack) {
        for (auto& env : *envStack) {
            if (env) env->gc_mark();
        }
    }

    // Root 4: Temporary roots (explicitly pinned objects)
    for (GCObject* root : tempRoots) {
        if (root && !root->gc_marked) root->gc_mark();
    }

    // Root 5: External roots (e.g. BytecodeVM stacks)
    for (IGCRoot* root : externalRoots) {
        if (root) root->gcMarkRoots();
    }

    // Phase 3: Sweep — break internal references of unmarked objects
    // Collect pointers first, since gc_clear() can trigger destructions that modify the list
    stillValid.clear();
    obj = head;
    while (obj) {
        stillValid.insert(obj);
        obj = obj->gc_next;
    }

    std::vector<GCObject*> toClear;
    obj = head;
    while (obj) {
        if (!obj->gc_marked) {
            toClear.push_back(obj);
        }
        obj = obj->gc_next;
    }

    for (GCObject* cand : toClear) {
        if (stillValid.count(cand)) {
            cand->gc_clear();
            unregisterObject(cand);
        }
    }

    stillValid.clear();
    
    // Resume-The-World
    gc_requested.store(false, std::memory_order_release);
    stw_cv.notify_all();
    
    isCollecting = false;
}

std::shared_ptr<Environment> Environment::createChild() {
    return std::make_shared<Environment>(shared_from_this());
}
