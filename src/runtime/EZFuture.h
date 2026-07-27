#ifndef EZFUTURE_H
#define EZFUTURE_H

// Windows-native future replacement for MinGW.
// std::promise/std::future cross-thread synchronization crashes when
// statically linking libstdc++ + libpthread on MinGW.
// This uses a Windows Event object on Windows, and std::condition_variable on non-Windows.

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <condition_variable>
#endif

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
#ifdef _WIN32
    HANDLE   hEvent;
#else
    std::condition_variable cv;
    bool     ready;
#endif
    mutable std::mutex mtx;
    Value*   result;  // heap-allocated copy of the result
    bool     hasError;
    std::string errorMsg;
    std::vector<std::function<void()>> onReady;

    EZFuture()
#ifdef _WIN32
        // manual-reset, initially non-signaled
        : hEvent(CreateEvent(nullptr, 1, 0, nullptr))
        , result(nullptr)
        , hasError(false)
    {
        if (!hEvent) {
            throw std::runtime_error("CreateEvent failed for EZFuture");
        }
    }
#else
        : ready(false)
        , result(nullptr)
        , hasError(false)
    {}
#endif

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
    void wait() {
#ifdef _WIN32
        WaitForSingleObject(hEvent, INFINITE);
#else
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]{ return ready; });
#endif
    }

    // Non-blocking poll.
    bool isReady() const {
#ifdef _WIN32
        return WaitForSingleObject(hEvent, 0) == WAIT_OBJECT_0;
#else
        std::lock_guard<std::mutex> lock(mtx);
        return ready;
#endif
    }

    // Register a callback to be executed when the future is ready.
    void then(std::function<void()> callback) {
        bool executeNow = false;
        {
            std::lock_guard<std::mutex> lock(mtx);
#ifdef _WIN32
            if (result || hasError) executeNow = true;
#else
            if (ready) executeNow = true;
#endif
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
#ifdef _WIN32
    if (hEvent) CloseHandle(hEvent);
#endif
    delete result;
}

void EZFuture::set(const Value& val) {
    std::vector<std::function<void()>> callbacks;
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (!result && !hasError) {
            result = new Value(val);
#ifndef _WIN32
            ready = true;
#endif
        }
        callbacks = std::move(onReady);
        onReady.clear();
    }
#ifdef _WIN32
    SetEvent(hEvent);
#else
    cv.notify_all();
#endif
    for (auto& cb : callbacks) if (cb) cb();
}

void EZFuture::setError(const std::string& msg) {
    std::vector<std::function<void()>> callbacks;
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (!result && !hasError) {
            hasError = true;
            errorMsg = msg;
#ifndef _WIN32
            ready = true;
#endif
        }
        callbacks = std::move(onReady);
        onReady.clear();
    }
#ifdef _WIN32
    SetEvent(hEvent);
#else
    cv.notify_all();
#endif
    for (auto& cb : callbacks) if (cb) cb();
}

Value EZFuture::get() {
    wait();
    std::lock_guard<std::mutex> lock(mtx);
    if (hasError) throw std::runtime_error(errorMsg);
    if (result) return *result;
    return Value();
}
#endif // EZFUTURE_IMPL

#endif // EZFUTURE_H

