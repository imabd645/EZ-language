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
#include <string>
#include <stdexcept>
#include <vector>

// Forward-declare Value to avoid circular includes. Full definition is in Value.h.
struct Value;

// Not copyable or movable — always use via pointer/shared_ptr
struct EZFuture {
    HANDLE   hEvent;
    std::mutex mtx;
    Value*   result;  // heap-allocated copy of the result
    bool     hasError;
    std::string errorMsg;
    std::vector<std::function<void()>> onReady;

    EZFuture()
        // manual-reset, initially non-signaled
        : hEvent(CreateEvent(nullptr, 1, 0, nullptr))
        , result(nullptr)
        , hasError(false)
    {
        if (!hEvent) {
            throw std::runtime_error("CreateEvent failed for EZFuture");
        }
    }

    ~EZFuture();

    EZFuture(const EZFuture&)            = delete;
    EZFuture& operator=(const EZFuture&) = delete;

    // Store a result and signal the event.
    void set(const Value& val);

    void setError(const std::string& msg);
    bool isError() const { return hasError; }
    std::string getError() const { return errorMsg; }

    // Cancel the future, making it throw "Cancelled"
    void cancel() { setError("Cancelled"); }

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
            if (result || hasError) executeNow = true;
            else onReady.push_back(std::move(callback));
        }
        if (executeNow) callback();
    }
};

// Implementations are provided after Value is fully defined.
// Include this only AFTER including Value.h.
#ifdef EZFUTURE_IMPL
#include "runtime/Value.h"  // provides full Value definition

EZFuture::~EZFuture() {
    if (hEvent) CloseHandle(hEvent);
    delete result;
}

void EZFuture::set(const Value& val) {
    std::vector<std::function<void()>> callbacks;
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (!result && !hasError) result = new Value(val);
        callbacks = std::move(onReady);
        onReady.clear();
    }
    SetEvent(hEvent);
    for (auto& cb : callbacks) if (cb) cb();
}

void EZFuture::setError(const std::string& msg) {
    std::vector<std::function<void()>> callbacks;
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (!result && !hasError) {
            hasError = true;
            errorMsg = msg;
        }
        callbacks = std::move(onReady);
        onReady.clear();
    }
    SetEvent(hEvent);
    for (auto& cb : callbacks) if (cb) cb();
}

Value EZFuture::get() {
    WaitForSingleObject(hEvent, INFINITE);
    std::lock_guard<std::mutex> lock(mtx);
    if (hasError) throw std::runtime_error(errorMsg);
    if (result) return *result;
    return Value();
}
#endif // EZFUTURE_IMPL

#endif // EZFUTURE_H
