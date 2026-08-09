#ifndef EZ_BUILTINS_FFI_INTERNAL_H
#define EZ_BUILTINS_FFI_INTERNAL_H

// ============================================================================
// Shared plumbing for the FFI builtins.
//
// The FFI layer used to be one 1200-line translation unit. It is split by
// concern now -- memory, calling, callbacks, structs -- and this header holds
// only what more than one of those files needs. Anything used by a single file
// stays `static` inside it.
//
// Layout:
//   FFI.cpp           registerFFIBuiltins() -- calls the four group registrars
//   FFI_Support.cpp   allocation registry, bounds checking, the SEH guard
//   FFI_Memory.cpp    os_alloc/os_free and every os_read_*/os_write_*
//   FFI_Call.cpp      os_load_lib/os_get_func/os_call/os_call_sig[_arr]
//   FFI_Callback.cpp  os_ffi_create_callback/free/is_callback, proxy WndProc
//   FFI_Struct.cpp    os_struct_alloc/pack/unpack
// ============================================================================

#include "runtime/objects/EZObjects.h"
#include "runtime/RuntimeContext.h"
#include "vm/BytecodeVM.h"
#include "builtins/Builtins.h"
#include "eventloop/EventLoop.h"

#include <ffi.h>
#include <unordered_map>
#include <map>
#include <mutex>
#include <thread>
#include <future>
#include <cstdint>
#include <iostream>
#include <chrono>
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
#endif

// EZFuture is Windows-native and must come after windows.h.
#include "runtime/EZFuture.h"

// ── Allocation registry ─────────────────────────────────────────────────────
// os_alloc blocks are tracked so os_read_*/os_write_* can reject an access that
// starts inside a managed block and runs past its end. Raw pointers handed back
// by native code are untracked and therefore unchecked -- see ffiBoundsCheck.
void ffiTrackAlloc(uintptr_t addr, size_t size);
bool ffiUntrackAlloc(uintptr_t addr);
bool ffiBoundsCheck(RuntimeContext& interp, uintptr_t base, size_t offset, size_t accessSize);

// ── Crash guard ─────────────────────────────────────────────────────────────
// A bad native call or pointer dereference must surface as a catchable EZ error
// rather than killing the process. MSVC uses __try/__except directly; MinGW has
// no SEH, so a vectored handler longjmps back out.
#ifdef _WIN32
#ifndef _MSC_VER
extern thread_local jmp_buf os_call_jmp_env;  // thread_local: safe under concurrent spawn()
LONG CALLBACK FfiVectoredHandler(PEXCEPTION_POINTERS ExceptionInfo);
#endif
#endif

// One-line guard for the read/write builtins. `retExpr` is what to return on a
// rejected access (nil for writes, a zero Value for reads).
#define FFI_BOUNDS(interp, args, accessSize, retExpr) \
    do { if (!ffiBoundsCheck((interp), (uintptr_t)(args)[0].asNumber(), \
                             (size_t)(args)[1].asNumber(), (accessSize))) return retExpr; } while(0)

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

// ── Group registrars ────────────────────────────────────────────────────────
// Called in order by registerFFIBuiltins(). The order is not significant --
// every name lands in the same global namespace -- but it is kept the same as
// the original single-file version so `ez --dump`-style listings do not shuffle.
void registerFFIMemory(RuntimeContext& interp);
void registerFFICall(RuntimeContext& interp);
void registerFFIStruct(RuntimeContext& interp);
void registerFFICallback(RuntimeContext& interp);

#endif // EZ_BUILTINS_FFI_INTERNAL_H
