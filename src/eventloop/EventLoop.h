#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include <queue>
#include <functional>
#include <mutex>
#include <atomic>

#ifdef TokenType
#undef TokenType
#define RESTORE_TOKEN_TYPE
#endif

#include <uv.h>

#ifdef _WIN32
#ifdef INTERFACE
#undef INTERFACE
#endif
#ifdef ERROR
#undef ERROR
#endif
#ifdef IN
#undef IN
#endif
#ifdef OUT
#undef OUT
#endif
#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif
#endif

#ifdef RESTORE_TOKEN_TYPE
#define TokenType TokenKind
#endif

class EventLoop {
public:
    // Magic static singleton pattern.
    static EventLoop& instance() {
        static EventLoop* loop = new EventLoop();
        return *loop;
    }

    // Push a task to be executed on the main event loop thread
    void pushTask(std::function<void()> task);

    // Run the loop
    void run();

    // Forcefully stop the event loop.
    void stop();

    // Prevent the loop from exiting (called when starting an async I/O operation)
    void retain();
    
    // Allow the loop to exit (called when an async I/O operation completes)
    void release();

    // Get the underlying uv_loop_t
    uv_loop_t* getLoop() { return uv_default_loop(); }

private:
    EventLoop();
    ~EventLoop() = default;
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    static void asyncCallback(uv_async_t* handle);

    std::queue<std::function<void()>> taskQueue;
    std::mutex queueMutex;
    
    uv_async_t asyncTaskHandle;

    int pendingIoCount = 0;
    std::atomic<bool> isRunning{false};
    std::atomic<bool> stopRequested{false};
};

#endif // EVENT_LOOP_H
