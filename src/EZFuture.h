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
#include <functional>

// Forward-declare Value to avoid circular includes. Full definition is in Value.h.
struct Value;

struct EZFuture {
    HANDLE   hEvent;
    std::mutex mtx;
    Value*   result;  // heap-allocated copy of the result
    std::function<void()> onReady;

    EZFuture()
        : hEvent(CreateEvent(nullptr, TRUE, FALSE, nullptr))
        , result(nullptr)
    {}

    ~EZFuture();

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

    // Register a callback to be executed when the future is ready.
    void then(std::function<void()> callback) {
        bool executeNow = false;
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (result) executeNow = true;
            else onReady = std::move(callback);
        }
        if (executeNow) callback();
    }
};

// Implementations are provided after Value is fully defined.
// Include this only AFTER including Value.h.
#ifdef EZFUTURE_IMPL
#include "Value.h"  // provides full Value definition

inline EZFuture::~EZFuture() {
    if (hEvent) CloseHandle(hEvent);
    delete result;
}

inline void EZFuture::set(const Value& val) {
    std::function<void()> callback;
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (!result) result = new Value(val);
        if (onReady) {
            callback = std::move(onReady);
            onReady = nullptr;
        }
    }
    SetEvent(hEvent);
    if (callback) callback();
}

inline Value EZFuture::get() {
    WaitForSingleObject(hEvent, INFINITE);
    std::lock_guard<std::mutex> lock(mtx);
    if (result) return *result;
    return Value();
}
#endif // EZFUTURE_IMPL

#endif // EZFUTURE_H
