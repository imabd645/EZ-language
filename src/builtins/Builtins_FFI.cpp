#include "runtime/objects/EZObjects.h"
#include "runtime/RuntimeContext.h"
#include "vm/BytecodeVM.h"
#include "runtime/EZFuture.h"
#include "builtins/Builtins.h"
#include "eventloop/EventLoop.h"
#include <ffi.h>
#include <unordered_map>
#include <map>
#include <mutex>
#include <thread>
#include <future>
#include <cstdint>

struct CallbackClosure {
    ffi_closure* closure;
    ffi_cif cif;
    std::vector<ffi_type*> argTypes;
    Value ezFunction;
    RuntimeContext* interp;
    std::vector<std::string> sigTypes;
    std::string retType;
};

static std::unordered_map<void*, CallbackClosure*> g_callbacks;
static std::mutex g_callbacksMutex;

// Capture the main thread ID when the application loads this translation unit
static std::thread::id g_mainThreadId = std::this_thread::get_id();

// ── FFI allocation registry ─────────────────────────────────────────────────
// Tracks blocks handed out by os_alloc so that:
//   * os_free can reject freeing an address it never allocated (or a double
//     free) — otherwise a script could corrupt the heap by free()ing an
//     arbitrary integer, and
//   * os_read_*/os_write_* can bounds-check accesses that land inside a managed
//     block.
// Raw pointers that did NOT come from os_alloc (e.g. returned by a C library)
// are intentionally not tracked: they stay usable for real FFI, protected only
// by the SEH/VEH guard, not by bounds checking.
static std::map<uintptr_t, size_t> g_ffiAllocs; // base address -> size
static std::mutex                  g_ffiAllocsMutex;

static void ffiTrackAlloc(uintptr_t addr, size_t size) {
    if (!addr) return;
    std::lock_guard<std::mutex> lock(g_ffiAllocsMutex);
    g_ffiAllocs[addr] = size;
}

// Remove a tracked allocation; returns true only if it was actually tracked
// (i.e. a legitimate free of an os_alloc block).
static bool ffiUntrackAlloc(uintptr_t addr) {
    std::lock_guard<std::mutex> lock(g_ffiAllocsMutex);
    auto it = g_ffiAllocs.find(addr);
    if (it == g_ffiAllocs.end()) return false;
    g_ffiAllocs.erase(it);
    return true;
}

// Bounds policy for os_read_*/os_write_*:
//   - access fully inside a tracked os_alloc block  -> allowed
//   - access starts inside a tracked block but runs past its end -> REJECTED
//     (the overflow we want to stop)
//   - access not inside any tracked block           -> allowed (raw FFI pointer)
// Returns false (after raising a runtime error) only for the overflow/overflowed
// -address cases.
static bool ffiBoundsCheck(RuntimeContext& interp, uintptr_t base, size_t offset, size_t accessSize) {
    uintptr_t addr, end;
    if (__builtin_add_overflow(base, (uintptr_t)offset, &addr) ||
        __builtin_add_overflow(addr, (uintptr_t)accessSize, &end)) {
        interp.runtimeError("FFI access address overflow", 0, "");
        return false;
    }
    std::lock_guard<std::mutex> lock(g_ffiAllocsMutex);
    if (g_ffiAllocs.empty()) return true;
    auto it = g_ffiAllocs.upper_bound(addr); // first block with start > addr
    if (it == g_ffiAllocs.begin()) return true; // addr below all managed blocks
    --it;
    uintptr_t blockStart = it->first;
    uintptr_t blockEnd   = blockStart + it->second;
    if (addr >= blockStart && addr < blockEnd) {
        if (end > blockEnd) {
            interp.runtimeError("FFI out-of-bounds access on managed allocation", 0, "");
            return false;
        }
        return true;
    }
    return true; // not inside a managed block -> raw pointer, allowed
}

// One-line guard for the read/write builtins. `retExpr` is what to return on a
// rejected access (nil for writes, a zero Value for reads).
#define FFI_BOUNDS(interp, args, accessSize, retExpr) \
    do { if (!ffiBoundsCheck((interp), (uintptr_t)(args)[0].asNumber(), \
                             (size_t)(args)[1].asNumber(), (accessSize))) return retExpr; } while(0)

static void ffi_callback_dispatcher(ffi_cif* cif, void* ret, void** args, void* user_data) {
    CallbackClosure* cb = static_cast<CallbackClosure*>(user_data);
    if (!cb || !cb->interp) return;

    std::vector<Value> ezArgs;
    for (size_t i = 0; i < cb->sigTypes.size(); ++i) {
        const std::string& type = cb->sigTypes[i];
        if (type == "int" || type == "ptr" || type == "i64" || type == "u64") {
            long long val = *(long long*)args[i];
            ezArgs.push_back(Value(val));
        } else if (type == "i32" || type == "u32") {
            long long val = *(int32_t*)args[i];
            ezArgs.push_back(Value(val));
        } else if (type == "float" || type == "f32") {
            double val = *(float*)args[i];
            ezArgs.push_back(Value(val));
        } else if (type == "double" || type == "f64") {
            double val = *(double*)args[i];
            ezArgs.push_back(Value(val));
        } else if (type == "string") {
            const char* str = *(const char**)args[i];
            ezArgs.push_back(str ? Value(std::string(str)) : Value(""));
        } else {
            ezArgs.push_back(Value((long long)*(intptr_t*)args[i]));
        }
    }

    Value result;
    if (std::this_thread::get_id() == g_mainThreadId) {
        result = cb->interp->callFunction(cb->ezFunction, ezArgs, 0, "ffi_callback");
    } else {
        std::promise<Value> p;
        auto f = p.get_future();
        EventLoop::instance().pushTask([cb, ezArgs, &p]() {
            try {
                p.set_value(cb->interp->callFunction(cb->ezFunction, ezArgs, 0, "ffi_callback"));
            } catch (...) {
                p.set_value(Value());
            }
        });
        result = f.get();
    }

    if (cb->retType == "int" || cb->retType == "ptr" || cb->retType == "i64" || cb->retType == "u64") {
        *(long long*)ret = result.isNumber() ? (long long)result.asNumber() : 0;
    } else if (cb->retType == "i32" || cb->retType == "u32") {
        *(int32_t*)ret = result.isNumber() ? (int32_t)result.asNumber() : 0;
    } else if (cb->retType == "float" || cb->retType == "f32") {
        *(float*)ret = result.isNumber() ? (float)result.asFloat() : 0.0f;
    } else if (cb->retType == "double" || cb->retType == "f64") {
        *(double*)ret = result.isNumber() ? result.asFloat() : 0.0;
    } else if (cb->retType == "string") {
        *(const char**)ret = nullptr;
    } else if (cb->retType == "void") {
        // do nothing
    } else {
        *(intptr_t*)ret = result.isNumber() ? (intptr_t)result.asNumber() : 0;
    }
}

#include <iostream>
#include <chrono>
#include <thread>
#include <future>
#include <vector>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#define NOCOMM
#include <windows.h>
#include <setjmp.h>
#include <conio.h>
#include <shellapi.h>

// Include EZFuture (Windows-native future, must be after windows.h)

#include "runtime/EZFuture.h"

#ifdef _WIN32

#ifdef _MSC_VER
static intptr_t do_ffi_call(void* funcPtr, intptr_t* cArgs, size_t argc, bool isFloat, double& f_ret, bool& crashed) {
    intptr_t ret = 0;
    crashed = false;
    __try {
        if (isFloat) {
            using fFunc0 = double(*)(); using fFunc1 = double(*)(intptr_t); using fFunc2 = double(*)(intptr_t, intptr_t);
            using fFunc3 = double(*)(intptr_t, intptr_t, intptr_t); using fFunc4 = double(*)(intptr_t, intptr_t, intptr_t, intptr_t);
            if (argc == 0) f_ret = ((fFunc0)funcPtr)();
            else if (argc == 1) f_ret = ((fFunc1)funcPtr)(cArgs[0]);
            else if (argc == 2) f_ret = ((fFunc2)funcPtr)(cArgs[0], cArgs[1]);
            else if (argc == 3) f_ret = ((fFunc3)funcPtr)(cArgs[0], cArgs[1], cArgs[2]);
            else if (argc >= 4) f_ret = ((fFunc4)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3]);
        } else {
            using Func0 = intptr_t(*)(); using Func1 = intptr_t(*)(intptr_t); using Func2 = intptr_t(*)(intptr_t, intptr_t);
            using Func3 = intptr_t(*)(intptr_t, intptr_t, intptr_t); using Func4 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t);
            using Func5 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t); using Func6 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t);
            using Func7 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t); using Func8 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t);
            using Func9 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t); using Func10 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t);
            using Func11 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t); using Func12 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t);
            if (argc == 0) ret = ((Func0)funcPtr)();
            else if (argc == 1) ret = ((Func1)funcPtr)(cArgs[0]);
            else if (argc == 2) ret = ((Func2)funcPtr)(cArgs[0], cArgs[1]);
            else if (argc == 3) ret = ((Func3)funcPtr)(cArgs[0], cArgs[1], cArgs[2]);
            else if (argc == 4) ret = ((Func4)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3]);
            else if (argc == 5) ret = ((Func5)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4]);
            else if (argc == 6) ret = ((Func6)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5]);
            else if (argc == 7) ret = ((Func7)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6]);
            else if (argc == 8) ret = ((Func8)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7]);
            else if (argc == 9) ret = ((Func9)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7], cArgs[8]);
            else if (argc == 10) ret = ((Func10)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7], cArgs[8], cArgs[9]);
            else if (argc == 11) ret = ((Func11)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7], cArgs[8], cArgs[9], cArgs[10]);
            else if (argc >= 12) ret = ((Func12)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7], cArgs[8], cArgs[9], cArgs[10], cArgs[11]);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        crashed = true;
    }
    return ret;
}
#else
static thread_local jmp_buf os_call_jmp_env; // thread_local: safe for concurrent spawn() FFI calls
static LONG CALLBACK FfiVectoredHandler(PEXCEPTION_POINTERS ExceptionInfo) {
    DWORD code = ExceptionInfo->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_ACCESS_VIOLATION ||
        code == EXCEPTION_ILLEGAL_INSTRUCTION ||
        code == EXCEPTION_INT_DIVIDE_BY_ZERO ||
        code == EXCEPTION_PRIV_INSTRUCTION) {
        longjmp(os_call_jmp_env, 1);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

// Map an EZ FFI type name to the libffi type used for a RETURN value.
//
// Getting this right is what lets libffi widen a sub-register return correctly.
// A C function that returns a 32-bit int leaves the upper 32 bits of the return
// register undefined, so reading the result as 64 bits gives garbage -- e.g. a
// returned -7 read back as 4294967289. Told the true type (ffi_type_sint32),
// libffi sign-extends it to the register width and the read is correct. Unsigned
// types are zero-extended. Anything unrecognised (including "int" and "i64")
// defaults to 64-bit signed, which is what EZ integers are.
static ffi_type* ffiReturnType(const std::string& t) {
    if (t == "void")                 return &ffi_type_void;
    // "float" means a 64-bit double here: EZ's number type IS a double, and the
    // FFI convention (and every existing caller, e.g. lib/db.ez reading
    // sqlite3_column_double as "float") has always treated it that way. Only the
    // explicit "f32" is a 32-bit C float.
    if (t == "f32")                  return &ffi_type_float;
    if (t == "float" || t == "double" || t == "f64") return &ffi_type_double;
    if (t == "i8")                   return &ffi_type_sint8;
    if (t == "u8")                   return &ffi_type_uint8;
    if (t == "i16")                  return &ffi_type_sint16;
    if (t == "u16")                  return &ffi_type_uint16;
    if (t == "i32")                  return &ffi_type_sint32;
    if (t == "u32")                  return &ffi_type_uint32;
    if (t == "u64")                  return &ffi_type_uint64;
    if (t == "ptr")                  return &ffi_type_pointer;
    return &ffi_type_sint64; // "int", "i64", and the default
}

// The register-sized buffer a libffi call writes its return value into. libffi
// widens every integer return to at least ffi_arg (8 bytes on x64) with the
// correct sign/zero extension for the ffi_type, so the integer members read back
// directly. float/double are stored at their own width.
union FfiRet { intptr_t i; uint64_t u; double d; float f; };

// Turn a completed FfiRet into an EZ Value according to the declared return type.
static Value ffiReadReturn(const std::string& retType, const FfiRet& r) {
    if (retType == "void")   return Value();
    // "f32" is a real 32-bit float; "float" (like "double"/"f64") is 64-bit.
    if (retType == "f32")    return Value((double)r.f);
    if (retType == "float" || retType == "double" || retType == "f64") return Value(r.d);
    if (retType == "string") {
        const char* s = reinterpret_cast<const char*>(r.i);
        return s ? Value(std::string(s)) : Value("");
    }
    // Unsigned reads: libffi already zero-extended, so the low bits are the
    // value; mask to be explicit. u64 above 2^63 cannot be represented in EZ's
    // signed 64-bit integer and comes back negative -- a documented limit.
    if (retType == "u8")  return Value((long long)(uint8_t)r.u);
    if (retType == "u16") return Value((long long)(uint16_t)r.u);
    if (retType == "u32") return Value((long long)(uint32_t)r.u);
    if (retType == "u64") return Value((long long)r.u);
    // Signed and pointer: libffi sign-extended smaller signed types already.
    return Value((long long)r.i);
}

// Marshal one EZ Value into an integer/pointer-register argument slot. Integers
// go through asInteger() -- NOT asNumber(), which is a double and silently loses
// precision above 2^53 (an int64 argument would arrive mangled). Floats still
// pass through here for os_call, which lacks per-argument typing; use os_call_sig
// for a genuine floating-point parameter.
static intptr_t ffiMarshalIntArg(const Value& v, std::string& tempStr) {
    if (v.isInteger())      return (intptr_t)v.asInteger();
    if (v.isFloat())        return (intptr_t)v.asFloat();
    if (v.isString())     { tempStr = v.asString(); return reinterpret_cast<intptr_t>(tempStr.c_str()); }
    if (v.isBuffer())       return reinterpret_cast<intptr_t>(v.asBuffer().data());
    if (v.isBool())         return v.asBool() ? 1 : 0;
    return 0;
}

static bool ffi_call_helper(void* funcPtr, size_t argc, ffi_type** argTypes, void** argValues, ffi_type* retType, void* retBuffer, RuntimeContext& interp) {
    ffi_cif cif;
    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, (unsigned int)argc, retType, argTypes) == FFI_OK) {
        bool crashed = false;
        PVOID vehHandler = nullptr;
#ifndef _MSC_VER
        vehHandler = AddVectoredExceptionHandler(1, FfiVectoredHandler);
#endif

#ifdef _MSC_VER
        __try {
#else
        if (setjmp(os_call_jmp_env) == 0) {
#endif
            ffi_call(&cif, reinterpret_cast<void(*)()>(funcPtr), retBuffer, argValues);
#ifdef _MSC_VER
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            crashed = true;
        }
#else
        } else {
            crashed = true;
        }
        RemoveVectoredExceptionHandler(vehHandler);
#endif
        if (crashed) {
            interp.runtimeError("os_call: Access violation or fatal memory fault inside external DLL.", 0, "");
            return false;
        }
        return true;
    }
    interp.runtimeError("os_call: Failed to prepare FFI CIF", 0, "");
    return false;
}
#endif
#endif

static LRESULT CALLBACK EZ_ProxyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Only redirect SENT messages (WM_COMMAND, WM_NOTIFY)
    // Redirect with 0x8000 offset to avoid infinite loop when DispatchMessage calls this proxy
    if (msg == WM_COMMAND || msg == WM_NOTIFY) {
        PostMessage(hwnd, msg + 0x8000, wParam, lParam);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
#endif

struct StructField {
    size_t size;
    size_t align;
    size_t offset;
    std::string type;
};

struct StructLayout {
    std::vector<StructField> fields;
    size_t totalSize;
    size_t maxAlign;
};

static bool parseStructType(const std::string& type, size_t& size, size_t& align) {
    if (type == "i8" || type == "u8") { size = 1; align = 1; return true; }
    if (type == "i16" || type == "u16") { size = 2; align = 2; return true; }
    if (type == "i32" || type == "u32" || type == "f32") { size = 4; align = 4; return true; }
    if (type == "i64" || type == "u64" || type == "f64" || type == "ptr") { size = 8; align = 8; return true; }
    return false;
}

static StructLayout computeStructLayout(const std::vector<Value>& layoutArray) {
    StructLayout layout;
    layout.totalSize = 0;
    layout.maxAlign = 1;

    for (const auto& val : layoutArray) {
        if (!val.isString()) continue;
        std::string type = val.asString();
        size_t size = 0, align = 0;
        if (!parseStructType(type, size, align)) {
            size = 1; align = 1; // Fallback
        }

        if (align > layout.maxAlign) layout.maxAlign = align;
        
        layout.totalSize = (layout.totalSize + align - 1) & ~(align - 1);
        
        StructField field;
        field.size = size;
        field.align = align;
        field.offset = layout.totalSize;
        field.type = type;
        layout.fields.push_back(field);
        
        layout.totalSize += size;
    }

    layout.totalSize = (layout.totalSize + layout.maxAlign - 1) & ~(layout.maxAlign - 1);
    return layout;
}


#ifdef _WIN32
#ifdef _MSC_VER
#define SAFE_MEMORY_OP(interp, op) \
    do { \
        bool __crashed = false; \
        __try { op; } \
        __except (EXCEPTION_EXECUTE_HANDLER) { __crashed = true; } \
        if (__crashed) { interp.runtimeError("FFI memory access violation", 0, ""); return Value(); } \
    } while(0)
#else
#define SAFE_MEMORY_OP(interp, op) \
    do { \
        bool __crashed = false; \
        PVOID __vehHandler = AddVectoredExceptionHandler(1, FfiVectoredHandler); \
        if (setjmp(os_call_jmp_env) == 0) { \
            op; \
        } else { \
            __crashed = true; \
        } \
        RemoveVectoredExceptionHandler(__vehHandler); \
        if (__crashed) { interp.runtimeError("FFI memory access violation", 0, ""); return Value(); } \
    } while(0)
#endif
#else
#define SAFE_MEMORY_OP(interp, op) do { op; } while(0)
#endif

void registerFFIBuiltins(RuntimeContext& interp) {
    interp.defineGlobal("os_alloc", Value::makeNativeFunction("os_alloc", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber()) { interp.runtimeError("os_alloc: size must be a number", 0, ""); return Value((long long)0); }
            // Use asInteger() not asNumber(): avoids double-precision loss for
            // sizes > 2^53 (e.g. 2^53+1 silently rounds to 2^53 via asNumber).
            long long reqd = args[0].asInteger();
            if (reqd <= 0) { interp.runtimeError("os_alloc: size must be positive", 0, ""); return Value((long long)0); }
            size_t size = (size_t)reqd;
            void* ptr = calloc(1, size);
            if (ptr) ffiTrackAlloc(reinterpret_cast<uintptr_t>(ptr), size);
            return Value((long long)(reinterpret_cast<uintptr_t>(ptr)));
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_free", Value::makeNativeFunction("os_free", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber()) return Value();
            uintptr_t addr = (uintptr_t)args[0].asNumber();
            if (!addr) return Value();
            // Only free memory we handed out via os_alloc. Freeing an arbitrary
            // or already-freed address is a heap-corruption / double-free bug.
            if (!ffiUntrackAlloc(addr)) {
                interp.runtimeError("os_free: address was not returned by os_alloc (or already freed)", 0, "");
                return Value();
            }
            free(reinterpret_cast<void*>(addr));
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_read_uint64", Value::makeNativeFunction("os_read_uint64", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0LL);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            uint64_t val = 0;
            SAFE_MEMORY_OP(interp, val = *(uint64_t*)(base + offset));
            return Value((long long)val);
#else
            return Value(0LL);
#endif
        }));

    // NOTE: os_write_uint64 is registered once below (line ~646) with FFI_BOUNDS.
    // The duplicate entry that was here has been removed.

    interp.defineGlobal("os_read_int64", Value::makeNativeFunction("os_read_int64", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0LL);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            int64_t val = 0;
            SAFE_MEMORY_OP(interp, val = *(int64_t*)(base + offset));
            return Value((long long)val);
#else
            return Value(0LL);
#endif
        }));

    interp.defineGlobal("os_write_int64", Value::makeNativeFunction("os_write_int64", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            FFI_BOUNDS(interp, args, sizeof(int64_t), Value());
            SAFE_MEMORY_OP(interp, *(int64_t*)(base + offset) = (int64_t)args[2].asNumber(););
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_read_uint32", Value::makeNativeFunction("os_read_uint32", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0LL);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            uint32_t val = 0;
            SAFE_MEMORY_OP(interp, val = *(uint32_t*)(base + offset));
            return Value((long long)val);
#else
            return Value(0LL);
#endif
        }));

    interp.defineGlobal("os_read_int32", Value::makeNativeFunction("os_read_int32", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0LL);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            int32_t val = 0;
            SAFE_MEMORY_OP(interp, val = *(int32_t*)(base + offset));
            return Value((long long)val);
#else
            return Value(0LL);
#endif
        }));

    interp.defineGlobal("os_write_uint32", Value::makeNativeFunction("os_write_uint32", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            FFI_BOUNDS(interp, args, sizeof(uint32_t), Value());
            SAFE_MEMORY_OP(interp, *(uint32_t*)(base + offset) = (uint32_t)args[2].asNumber(););
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_write_int32", Value::makeNativeFunction("os_write_int32", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            FFI_BOUNDS(interp, args, sizeof(int32_t), Value());
            SAFE_MEMORY_OP(interp, *(int32_t*)(base + offset) = (int32_t)args[2].asNumber(););
            return Value();
#else
            return Value();
#endif
        }));



    interp.defineGlobal("os_get_proxy_wndproc", Value::makeNativeFunction("os_get_proxy_wndproc", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            return Value((long long)(uintptr_t)EZ_ProxyWndProc);
#else
            return Value(0LL);
#endif
        }));

    interp.defineGlobal("os_read_uint16", Value::makeNativeFunction("os_read_uint16", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0LL);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            uint16_t val = 0;
            SAFE_MEMORY_OP(interp, val = *(uint16_t*)(base + offset));
            return Value((long long)val);
#else
            return Value(0LL);
#endif
        }));

    interp.defineGlobal("os_read_int16", Value::makeNativeFunction("os_read_int16", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0LL);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            int16_t val = 0;
            SAFE_MEMORY_OP(interp, val = *(int16_t*)(base + offset));
            return Value((long long)val);
#else
            return Value(0LL);
#endif
        }));

    interp.defineGlobal("os_write_uint16", Value::makeNativeFunction("os_write_uint16", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            FFI_BOUNDS(interp, args, sizeof(uint16_t), Value());
            SAFE_MEMORY_OP(interp, *(uint16_t*)(base + offset) = (uint16_t)args[2].asNumber(););
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_write_int16", Value::makeNativeFunction("os_write_int16", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            FFI_BOUNDS(interp, args, sizeof(int16_t), Value());
            SAFE_MEMORY_OP(interp, *(int16_t*)(base + offset) = (int16_t)args[2].asNumber(););
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_write_uint64", Value::makeNativeFunction("os_write_uint64", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            FFI_BOUNDS(interp, args, sizeof(uint64_t), Value());
            SAFE_MEMORY_OP(interp, *(uint64_t*)(base + offset) = (uint64_t)args[2].asNumber(););
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_read_float32", Value::makeNativeFunction("os_read_float32", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0.0);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            float val = 0;
            SAFE_MEMORY_OP(interp, val = *(float*)(base + offset));
            return Value((double)val);
#else
            return Value(0.0);
#endif
        }));

    interp.defineGlobal("os_write_float32", Value::makeNativeFunction("os_write_float32", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            FFI_BOUNDS(interp, args, sizeof(float), Value());
            SAFE_MEMORY_OP(interp, *(float*)(base + offset) = (float)args[2].asFloat(););
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_read_float64", Value::makeNativeFunction("os_read_float64", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0.0);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            double val = 0;
            SAFE_MEMORY_OP(interp, val = *(double*)(base + offset));
            return Value(val);
#else
            return Value(0.0);
#endif
        }));

    interp.defineGlobal("os_write_float64", Value::makeNativeFunction("os_write_float64", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            FFI_BOUNDS(interp, args, sizeof(double), Value());
            SAFE_MEMORY_OP(interp, *(double*)(base + offset) = args[2].asFloat(););
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_read_float", Value::makeNativeFunction("os_read_float", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0.0);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            double val = 0;
            SAFE_MEMORY_OP(interp, val = *(double*)(base + offset));
            return Value(val);
#else
            return Value(0.0);
#endif
        }));

    interp.defineGlobal("os_write_float", Value::makeNativeFunction("os_write_float", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            FFI_BOUNDS(interp, args, sizeof(double), Value());
            SAFE_MEMORY_OP(interp, *(double*)(base + offset) = args[2].asFloat(););
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_read_double", Value::makeNativeFunction("os_read_double", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0.0);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            double val = 0;
            SAFE_MEMORY_OP(interp, val = *(double*)(base + offset));
            return Value(val);
#else
            return Value(0.0);
#endif
        }));

    interp.defineGlobal("os_write_double", Value::makeNativeFunction("os_write_double", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            FFI_BOUNDS(interp, args, sizeof(double), Value());
            SAFE_MEMORY_OP(interp, *(double*)(base + offset) = args[2].asFloat(););
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_read_byte", Value::makeNativeFunction("os_read_byte", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0LL);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            uint8_t val = 0;
            SAFE_MEMORY_OP(interp, val = *(base + offset));
            return Value((long long)val);
#else
            return Value(0LL);
#endif
        }));

    interp.defineGlobal("os_write_byte", Value::makeNativeFunction("os_write_byte", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            FFI_BOUNDS(interp, args, sizeof(uint8_t), Value());
            SAFE_MEMORY_OP(interp, *(base + offset) = (uint8_t)args[2].asNumber());
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_read_string_ptr", Value::makeNativeFunction("os_read_string_ptr", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber()) return Value("");
            const char* str = reinterpret_cast<const char*>((uintptr_t)args[0].asNumber());
            if (!str) return Value("");
            std::string res;
            SAFE_MEMORY_OP(interp, res = std::string(str));
            return Value(res);
#else
            return Value("");
#endif
        }));

    interp.defineGlobal("os_write_string", Value::makeNativeFunction("os_write_string", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isString()) return Value();
            char* base = reinterpret_cast<char*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            std::string text = args[2].asString();
            // Include the trailing NUL in the bounds check so the whole memcpy is
            // validated against the destination allocation.
            FFI_BOUNDS(interp, args, text.length() + 1, Value());
            SAFE_MEMORY_OP(interp, memcpy(base + offset, text.c_str(), text.length() + 1));
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_load_lib", Value::makeNativeFunction("os_load_lib", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isString()) return Value();
            HMODULE handle = LoadLibraryA(args[0].asString().c_str());
            return Value((long long)(reinterpret_cast<uintptr_t>(handle)));
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_get_func", Value::makeNativeFunction("os_get_func", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isString()) return Value();
            HMODULE handle = reinterpret_cast<HMODULE>((uintptr_t)args[0].asNumber());
            FARPROC proc = GetProcAddress(handle, args[1].asString().c_str());
            return Value((long long)(reinterpret_cast<uintptr_t>(proc)));
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_call", Value::makeNativeFunction("os_call", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (args.size() < 2 || !args[0].isNumber() || !args[1].isString()) return Value();
            void* funcPtr = reinterpret_cast<void*>((uintptr_t)args[0].asNumber());
            if (!funcPtr) {
                interp.runtimeError("os_call: Null function pointer or function not found.", 0, "");
                return Value();
            }
            
            size_t argc = args.size() - 2;
            std::string retType = args[1].asString();
            
            // Every argument occupies one integer/pointer register slot. os_call
            // has no per-argument typing, so a real floating-point parameter must
            // go through os_call_sig; here integers are marshalled EXACTLY (via
            // asInteger(), not a lossy double).
            std::vector<ffi_type*> argTypes(argc, &ffi_type_pointer);
            std::vector<void*> argValues(argc);
            std::vector<intptr_t> cArgs(argc, 0);
            std::vector<std::string> tempStrings(argc);

            for (size_t i = 0; i < argc; i++) {
                cArgs[i] = ffiMarshalIntArg(args[i + 2], tempStrings[i]);
                argValues[i] = &cArgs[i];
            }

            ffi_type* rType = ffiReturnType(retType);
            FfiRet retBuffer; retBuffer.i = 0;

            if (!ffi_call_helper(funcPtr, argc, argTypes.data(), argValues.data(), rType, &retBuffer, interp)) {
                return Value();
            }
            return ffiReadReturn(retType, retBuffer);
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_call_sig", Value::makeNativeFunction("os_call_sig", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (args.size() < 3 || !args[0].isNumber() || !args[1].isString() || !args[2].isArray()) return Value();
            void* funcPtr = reinterpret_cast<void*>((uintptr_t)args[0].asNumber());
            if (!funcPtr) {
                interp.runtimeError("os_call_sig: Null function pointer.", 0, "");
                return Value();
            }
            
            std::string retType = args[1].asString();
            auto sigArray = args[2].asArray().getElementsCopy();
            size_t argc = sigArray.size();
            
            std::vector<ffi_type*> argTypes(argc);
            std::vector<void*> argValues(argc);
            std::vector<intptr_t> iArgs(argc, 0);
            std::vector<double> fArgs(argc, 0.0);
            std::vector<std::string> tempStrings(argc);
            
            for (size_t i = 0; i < argc; i++) {
                size_t valIdx = i + 3;
                if (valIdx >= args.size()) break;

                std::string type = sigArray[i].isString() ? sigArray[i].asString() : "ptr";

                if (type == "float" || type == "double" || type == "f32" || type == "f64") {
                    // A double parameter must land in an XMM register: give it the
                    // real floating type, not a reinterpreted integer.
                    argTypes[i] = &ffi_type_double;
                    if (args[valIdx].isInteger())    fArgs[i] = (double)args[valIdx].asInteger();
                    else if (args[valIdx].isNumber()) fArgs[i] = args[valIdx].asFloat();
                    argValues[i] = &fArgs[i];
                } else {
                    // Integers marshalled exactly (asInteger(), not a lossy double).
                    argTypes[i] = &ffi_type_pointer;
                    iArgs[i] = ffiMarshalIntArg(args[valIdx], tempStrings[i]);
                    argValues[i] = &iArgs[i];
                }
            }

            ffi_type* rType = ffiReturnType(retType);
            FfiRet retBuffer; retBuffer.i = 0;

            if (!ffi_call_helper(funcPtr, argc, argTypes.data(), argValues.data(), rType, &retBuffer, interp)) {
                return Value();
            }
            return ffiReadReturn(retType, retBuffer);
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_struct_alloc", Value::makeNativeFunction("os_struct_alloc", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                interp.runtimeError("os_struct_alloc expects an array of strings", 0, "");
                return Value();
            }
            auto layoutArray = args[0].asArray().getElementsCopy();
            StructLayout layout = computeStructLayout(layoutArray);
            return Value(std::make_shared<EZBuffer>(layout.totalSize));
        }));

    interp.defineGlobal("os_struct_pack", Value::makeNativeFunction("os_struct_pack", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.size() < 2 || !args[0].isArray() || !args[1].isArray()) {
                interp.runtimeError("os_struct_pack expects (layout_array, values_array, [buffer])", 0, "");
                return Value();
            }
            auto layoutArray = args[0].asArray().getElementsCopy();
            auto valuesArray = args[1].asArray().getElementsCopy();
            StructLayout layout = computeStructLayout(layoutArray);
            
            Value bufVal;
            if (args.size() >= 3 && args[2].isBuffer()) {
                bufVal = args[2];
                if (bufVal.asBuffer().size() < layout.totalSize) {
                    interp.runtimeError("os_struct_pack: buffer too small", 0, "");
                    return Value();
                }
            } else {
                bufVal = Value(std::make_shared<EZBuffer>(layout.totalSize));
            }
            
            uint8_t* base = bufVal.asBuffer().data();
            for (size_t i = 0; i < layout.fields.size() && i < valuesArray.size(); i++) {
                const auto& field = layout.fields[i];
                Value val = valuesArray[i];
                uint8_t* ptr = base + field.offset;
                
                if (field.type == "i8" || field.type == "u8") {
                    *ptr = (uint8_t)(val.isNumber() ? val.asNumber() : 0);
                } else if (field.type == "i16" || field.type == "u16") {
                    *(uint16_t*)ptr = (uint16_t)(val.isNumber() ? val.asNumber() : 0);
                } else if (field.type == "i32" || field.type == "u32") {
                    *(uint32_t*)ptr = (uint32_t)(val.isNumber() ? val.asNumber() : 0);
                } else if (field.type == "i64" || field.type == "u64" || field.type == "ptr") {
                    *(uint64_t*)ptr = (uint64_t)(val.isNumber() ? val.asNumber() : 0);
                } else if (field.type == "f32") {
                    *(float*)ptr = (float)(val.isNumber() ? val.asFloat() : 0.0f);
                } else if (field.type == "f64") {
                    *(double*)ptr = (double)(val.isNumber() ? val.asFloat() : 0.0);
                }
            }
            
            return bufVal;
        }));

    interp.defineGlobal("os_struct_unpack", Value::makeNativeFunction("os_struct_unpack", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                interp.runtimeError("os_struct_unpack expects (layout_array, buffer_or_ptr)", 0, "");
                return Value();
            }
            auto layoutArray = args[0].asArray().getElementsCopy();
            StructLayout layout = computeStructLayout(layoutArray);
            
            uint8_t* base = nullptr;
            if (args[1].isBuffer()) {
                if (args[1].asBuffer().size() < layout.totalSize) {
                    interp.runtimeError("os_struct_unpack: buffer too small", 0, "");
                    return Value();
                }
                base = args[1].asBuffer().data();
            } else if (args[1].isNumber()) {
                base = reinterpret_cast<uint8_t*>((uintptr_t)args[1].asNumber());
            } else {
                interp.runtimeError("os_struct_unpack: expects buffer or pointer address", 0, "");
                return Value();
            }
            
            if (!base) return Value::makeArray({});

            std::vector<Value> results;
            SAFE_MEMORY_OP(interp, {
            for (size_t i = 0; i < layout.fields.size(); i++) {
                const auto& field = layout.fields[i];
                uint8_t* ptr = base + field.offset;
                
                if (field.type == "i8") {
                    results.push_back(Value((long long)*(int8_t*)ptr));
                } else if (field.type == "u8") {
                    results.push_back(Value((long long)*(uint8_t*)ptr));
                } else if (field.type == "i16") {
                    results.push_back(Value((long long)*(int16_t*)ptr));
                } else if (field.type == "u16") {
                    results.push_back(Value((long long)*(uint16_t*)ptr));
                } else if (field.type == "i32") {
                    results.push_back(Value((long long)*(int32_t*)ptr));
                } else if (field.type == "u32") {
                    results.push_back(Value((long long)*(uint32_t*)ptr));
                } else if (field.type == "i64" || field.type == "u64" || field.type == "ptr") {
                    results.push_back(Value((long long)*(uint64_t*)ptr));
                } else if (field.type == "f32") {
                    results.push_back(Value((double)*(float*)ptr));
                } else if (field.type == "f64") {
                    results.push_back(Value((double)*(double*)ptr));
                } else {
                    results.push_back(Value());
                }
            }
            });
            
            return Value::makeArray(results);
        }));

    interp.defineGlobal("os_ffi_create_callback", Value::makeNativeFunction("os_ffi_create_callback", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) { interp.runtimeError("arg 0 must be array of strings", 0, ""); return Value(); }
            std::vector<std::string> sigTypes;
            for (auto& v : args[0].asArray().getElementsCopy()) sigTypes.push_back(v.toString());
            std::string retType = args[1].toString();
            Value ezFunction = args[2];

            CallbackClosure* cb = new CallbackClosure();
            cb->ezFunction = ezFunction;
            cb->interp = &interp;
            cb->sigTypes = sigTypes;
            cb->retType = retType;

            for(auto t : sigTypes) {
                if (t == "int" || t == "ptr" || t == "i64" || t == "u64") cb->argTypes.push_back(&ffi_type_sint64);
                else if (t == "i32" || t == "u32") cb->argTypes.push_back(&ffi_type_sint32);
                else if (t == "float" || t == "f32") cb->argTypes.push_back(&ffi_type_float);
                else if (t == "double" || t == "f64") cb->argTypes.push_back(&ffi_type_double);
                else if (t == "string") cb->argTypes.push_back(&ffi_type_pointer);
                else cb->argTypes.push_back(&ffi_type_pointer);
            }

            ffi_type* ffiRetType = &ffi_type_void;
            if (retType == "int" || retType == "ptr" || retType == "i64" || retType == "u64") ffiRetType = &ffi_type_sint64;
            else if (retType == "i32" || retType == "u32") ffiRetType = &ffi_type_sint32;
            else if (retType == "float" || retType == "f32") ffiRetType = &ffi_type_float;
            else if (retType == "double" || retType == "f64") ffiRetType = &ffi_type_double;
            else if (retType == "string") ffiRetType = &ffi_type_pointer;

            if (ffi_prep_cif(&cb->cif, FFI_DEFAULT_ABI, cb->argTypes.size(), ffiRetType, cb->argTypes.data()) != FFI_OK) {
                delete cb;
                interp.runtimeError("ffi_prep_cif failed", 0, "");
                return Value();
            }

            void* bound_ptr = nullptr;
            cb->closure = (ffi_closure*)ffi_closure_alloc(sizeof(ffi_closure), &bound_ptr);
            if (!cb->closure) {
                delete cb;
                interp.runtimeError("ffi_closure_alloc failed", 0, "");
                return Value();
            }

            if (ffi_prep_closure_loc(cb->closure, &cb->cif, ffi_callback_dispatcher, cb, bound_ptr) != FFI_OK) {
                ffi_closure_free(cb->closure);
                delete cb;
                interp.runtimeError("ffi_prep_closure_loc failed", 0, "");
                return Value();
            }

            {
                std::lock_guard<std::mutex> lock(g_callbacksMutex);
                g_callbacks[bound_ptr] = cb;
            }

            return Value((long long)(intptr_t)bound_ptr);
        }));

    interp.defineGlobal("os_ffi_free_callback", Value::makeNativeFunction("os_ffi_free_callback", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            void* ptr = (void*)(intptr_t)args[0].asNumber();
            std::lock_guard<std::mutex> lock(g_callbacksMutex);
            auto it = g_callbacks.find(ptr);
            if (it != g_callbacks.end()) {
                ffi_closure_free(it->second->closure);
                delete it->second;
                g_callbacks.erase(it);
            }
            return Value();
        }));

    // ── Production-ready additions ────────────────────────────────────────────

    // Bounded C-string read: stops at NUL or maxlen bytes, whichever comes first.
    // Prevents runaway reads on un-terminated foreign strings.
    interp.defineGlobal("os_read_string_ptr_n", Value::makeNativeFunction("os_read_string_ptr_n", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value("");
            const char* s = reinterpret_cast<const char*>((uintptr_t)args[0].asNumber());
            size_t maxLen = (size_t)args[1].asInteger();
            if (!s) return Value("");
            std::string res;
            SAFE_MEMORY_OP(interp, res = std::string(s, strnlen(s, maxLen)));
            return Value(res);
#else
            return Value("");
#endif
        }));

    // Lets EZ-level Pointer.free() distinguish callback trampolines (ffi_closure)
    // from plain os_alloc blocks so it can dispatch correctly.
    interp.defineGlobal("os_is_callback", Value::makeNativeFunction("os_is_callback", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) return Value(false);
            void* ptr = (void*)(intptr_t)args[0].asNumber();
            std::lock_guard<std::mutex> lock(g_callbacksMutex);
            return Value(g_callbacks.count(ptr) > 0);
        }));

    // os_call_sig_arr(funcPtr, retType, sigArray, argsArray)
    // Fully variadic os_call_sig variant: takes user args as an EZ array so
    // there is no N-argument limit at the EZ library level.
    interp.defineGlobal("os_call_sig_arr", Value::makeNativeFunction("os_call_sig_arr", 4,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isString() || !args[2].isArray() || !args[3].isArray()) {
                interp.runtimeError("os_call_sig_arr expects (ptr, retType, sigArray, argsArray)", 0, "");
                return Value();
            }
            void* funcPtr = reinterpret_cast<void*>((uintptr_t)args[0].asNumber());
            if (!funcPtr) { interp.runtimeError("os_call_sig_arr: null function pointer", 0, ""); return Value(); }

            std::string retType  = args[1].asString();
            auto sigArray        = args[2].asArray().getElementsCopy();
            auto userArgs        = args[3].asArray().getElementsCopy();
            size_t argc          = sigArray.size();

            std::vector<ffi_type*>   argTypes(argc);
            std::vector<void*>       argValues(argc);
            std::vector<intptr_t>    iArgs(argc, 0);
            std::vector<double>      fArgs(argc, 0.0);
            std::vector<std::string> tempStrings(argc);

            for (size_t i = 0; i < argc; i++) {
                std::string type = sigArray[i].isString() ? sigArray[i].asString() : "ptr";
                const Value& val = (i < userArgs.size()) ? userArgs[i] : Value();

                if (type == "float" || type == "double" || type == "f32" || type == "f64") {
                    argTypes[i]  = &ffi_type_double;
                    if (val.isInteger())     fArgs[i] = (double)val.asInteger();
                    else if (val.isNumber()) fArgs[i] = val.asFloat();
                    argValues[i] = &fArgs[i];
                } else {
                    argTypes[i]  = &ffi_type_pointer;
                    iArgs[i]     = ffiMarshalIntArg(val, tempStrings[i]);
                    argValues[i] = &iArgs[i];
                }
            }

            ffi_type* rType = ffiReturnType(retType);
            FfiRet retBuffer; retBuffer.i = 0;
            if (!ffi_call_helper(funcPtr, argc, argTypes.data(), argValues.data(), rType, &retBuffer, interp))
                return Value();
            return ffiReadReturn(retType, retBuffer);
#else
            return Value();
#endif
        }));
}

