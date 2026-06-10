#ifndef EZFUTURE_H
#define EZFUTURE_H

// Windows-native future replacement for MinGW.
// std::promise/std::future cross-thread synchronization crashes when
// statically linking libstdc++ + libpthread on MinGW.
// This uses a Windows Event object which is rock-solid on all Windows versions.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mutex>
#include <memory>

// Forward-declare Value to avoid circular includes. Full definition is in Value.h.
struct Value;

struct EZFuture {
    HANDLE   hEvent;
    std::mutex mtx;
    Value*   result;  // heap-allocated copy of the result

    EZFuture()
        : hEvent(CreateEvent(nullptr, TRUE, FALSE, nullptr))
        , result(nullptr)
    {}

    ~EZFuture() {
        if (hEvent) CloseHandle(hEvent);
        delete result;
    }

    EZFuture(const EZFuture&)            = delete;
    EZFuture& operator=(const EZFuture&) = delete;

    // Store a result and signal the event.
    void set(const Value& val);

    // Block until ready, then return the stored result.
    Value get();

    // Block until ready.
    void wait() { WaitForSingleObject(hEvent, INFINITE); }

    // Non-blocking poll.
    bool isReady() const { return WaitForSingleObject(hEvent, 0) == WAIT_OBJECT_0; }
};

// Implementations are provided after Value is fully defined.
// Include this only AFTER including Value.h.
#ifdef EZFUTURE_IMPL
#include "Value.h"  // provides full Value definition

inline void EZFuture::set(const Value& val) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (!result) result = new Value(val);
    }
    SetEvent(hEvent);
}

inline Value EZFuture::get() {
    WaitForSingleObject(hEvent, INFINITE);
    std::lock_guard<std::mutex> lock(mtx);
    if (result) return *result;
    return Value();
}
#endif // EZFUTURE_IMPL

#endif // EZFUTURE_H
