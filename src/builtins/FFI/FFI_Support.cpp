#include "FFI_Internal.h"

// ============================================================================
// FFI support: the os_alloc registry, its bounds policy, and the crash guard.
// Declared in FFI_Internal.h because memory, struct and call code all rely on
// them; everything else in the FFI layer keeps internal linkage.
// ============================================================================

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

void ffiTrackAlloc(uintptr_t addr, size_t size) {
    if (!addr) return;
    std::lock_guard<std::mutex> lock(g_ffiAllocsMutex);
    g_ffiAllocs[addr] = size;
}

// Remove a tracked allocation; returns true only if it was actually tracked
// (i.e. a legitimate free of an os_alloc block).
bool ffiUntrackAlloc(uintptr_t addr) {
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
bool ffiBoundsCheck(RuntimeContext& interp, uintptr_t base, size_t offset, size_t accessSize) {
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

// See the header for why this clamps rather than rejects.
size_t ffiClampToBlock(uintptr_t addr, size_t requested) {
    std::lock_guard<std::mutex> lock(g_ffiAllocsMutex);
    if (g_ffiAllocs.empty()) return requested;
    auto it = g_ffiAllocs.upper_bound(addr); // first block starting after addr
    if (it == g_ffiAllocs.begin()) return requested;
    --it;
    uintptr_t blockStart = it->first;
    uintptr_t blockEnd   = blockStart + it->second;
    if (addr >= blockStart && addr < blockEnd) {
        size_t available = (size_t)(blockEnd - addr);
        return requested < available ? requested : available;
    }
    return requested; // not in a managed block -> raw pointer, caller's business
}

#ifdef _WIN32
#ifndef _MSC_VER
thread_local jmp_buf os_call_jmp_env; // thread_local: safe for concurrent spawn() FFI calls
LONG CALLBACK FfiVectoredHandler(PEXCEPTION_POINTERS ExceptionInfo) {
    DWORD code = ExceptionInfo->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_ACCESS_VIOLATION ||
        code == EXCEPTION_ILLEGAL_INSTRUCTION ||
        code == EXCEPTION_INT_DIVIDE_BY_ZERO ||
        code == EXCEPTION_PRIV_INSTRUCTION) {
        longjmp(os_call_jmp_env, 1);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif
#else
// ── POSIX crash guard ───────────────────────────────────────────────────────
// Uses sigaction(SIGSEGV/SIGBUS) + sigsetjmp/siglongjmp to recover from bad
// pointer dereferences inside FFI memory operations. The guard is only active
// when posix_ffi_guard_active is set, so normal program crashes still produce
// the expected core dump / signal termination.
thread_local sigjmp_buf posix_ffi_jmp_env;
thread_local volatile sig_atomic_t posix_ffi_guard_active = 0;

static void posixFfiSignalHandler(int sig) {
    if (posix_ffi_guard_active) {
        posix_ffi_guard_active = 0;
        siglongjmp(posix_ffi_jmp_env, 1);
    }
    // Not inside a guarded region — re-raise with the default handler so the
    // process terminates normally (core dump, debugger attachment, etc.).
    signal(sig, SIG_DFL);
    raise(sig);
}

void posixFfiInstallHandlers() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = posixFfiSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS,  &sa, nullptr);
}
#endif

