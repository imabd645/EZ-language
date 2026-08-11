// Runtime.cpp
// Miscellaneous out-of-line definitions that depend on fully-defined types
// (Environment, EZFunction, etc.). Formerly split across GC.cpp.

#include "runtime/Environment.h"
#include "runtime/Value.h"
#include "gc/CycleCollector.h"
#include "runtime/objects/EZShape.h"

// ── Thread-local string interning pool ───────────────────────────────────────
//
// Only the main thread interns. Destroying this map during thread teardown
// crashes the process, because the strings it hands out outlive the thread that
// made them -- see the long note in ValueImpl.h.
//
// Interning is therefore OPT-IN, and exactly one thread opts in:
// ez_enable_string_interning() is called once from cli_main().
//
// It used to default to true, with each thread-creation site responsible for
// turning it off (spawn() workers did, Timer callbacks did). That only covers
// threads this codebase creates. The web server's worker threads are created
// inside http_accel.dll and run EZ code through an FFI callback, so they interned
// happily and then faulted in ~unordered_map on the way out:
//     #0  std::unordered_map<...>::~unordered_map()
//     #1  run_dtor_list (tls_atexit.c)
//     #8  ntdll!LdrShutdownThread
//     #11 pthread_create_wrapper  from http_accel.dll
// Defaulting to false makes every thread safe by construction -- including ones
// created by a library we do not control, and any added later.
thread_local std::unordered_map<std::string, std::weak_ptr<std::string>> globalStringPool;
thread_local bool g_stringInternEnabled = false;

// Turn interning on for the calling thread. Only ever called for the main
// thread, whose pool is torn down at process exit like any other global.
void ez_enable_string_interning() {
    g_stringInternEnabled = true;
}

// ── EZFunction::traverse() ───────────────────────────────────────────────────
// Declared in Value.h; defined here because Environment is not yet complete
// when Value.h is being parsed.
// EZFunction holds closure/staticEnv as shared_ptr<Environment>, NOT as Value,
// so there are no Value edges to traverse. (Environment scope chains cannot
// form GC-relevant cycles because they are always rooted by the call stack.)
void EZFunction::traverse(const ValueVisitor& /*visit*/) const {}

// ── Environment::createChild() ───────────────────────────────────────────────
std::shared_ptr<Environment> Environment::createChild() {
    return std::make_shared<Environment>(shared_from_this());
}

// ── EZClass::EZClass() ───────────────────────────────────────────────────────
EZClass::EZClass(const std::string& name) : name(name), parent(nullptr) {
    initialShape = std::make_shared<EZShape>();
}
