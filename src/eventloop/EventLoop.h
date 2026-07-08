#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>

class EventLoop {
public:
    // Magic static singleton pattern.
    // Intentionally leaked to avoid static destruction order hazards on exit.
    static EventLoop& instance() {
        static EventLoop* loop = new EventLoop();
        return *loop;
    }

    // Push a task to be executed on the main event loop thread
    void pushTask(std::function<void()> task);

    // Run the loop until taskQueue is empty AND pendingIoCount is 0, or stop() is called.
    // Throws std::runtime_error if called re-entrantly or concurrently.
    void run();

    // Forcefully stop the event loop.
    void stop() {
        stopRequested = true;
        cv.notify_all();
    }

    // Prevent the loop from exiting (called when starting an async I/O operation)
    void retain();
    
    // Allow the loop to exit (called when an async I/O operation completes)
    void release();

private:
    EventLoop() = default;
    ~EventLoop() = default;
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    std::queue<std::function<void()>> taskQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
    
    // Number of active I/O operations or Futures that the loop is waiting for
    int pendingIoCount = 0;

    std::atomic<bool> isRunning{false};
    std::atomic<bool> stopRequested{false};
};

#endif // EVENT_LOOP_H
