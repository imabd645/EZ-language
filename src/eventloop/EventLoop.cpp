#include "eventloop/EventLoop.h"
#include <iostream>
#include "runtime/Environment.h"
#define EZFUTURE_IMPL
#include "runtime/EZFuture.h"

EventLoop::EventLoop() {
    uv_async_init(uv_default_loop(), &asyncTaskHandle, asyncCallback);
    asyncTaskHandle.data = this;
    uv_unref(reinterpret_cast<uv_handle_t*>(&asyncTaskHandle));
}

void EventLoop::asyncCallback(uv_async_t* handle) {
    EventLoop* loop = static_cast<EventLoop*>(handle->data);
    
    std::function<void()> task;
    while (true) {
        {
            std::lock_guard<std::mutex> lock(loop->queueMutex);
            if (loop->taskQueue.empty()) break;
            task = std::move(loop->taskQueue.front());
            loop->taskQueue.pop();
        }
        
        if (task) {
            try {
                task();
            } catch (const RuntimeError& e) {
                std::cerr << "[EventLoop] Uncaught RuntimeError: " << e.what() << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[EventLoop] Uncaught std::exception: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[EventLoop] Uncaught unknown exception" << std::endl;
            }
        }
    }
}

void EventLoop::pushTask(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        taskQueue.push(std::move(task));
    }
    uv_async_send(&asyncTaskHandle);
}

void EventLoop::retain() {
    std::lock_guard<std::mutex> lock(queueMutex);
    pendingIoCount++;
}

void EventLoop::release() {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        pendingIoCount--;
        if (pendingIoCount < 0) {
            std::cerr << "[EventLoop] WARNING: pendingIoCount underflow (double release detected)." << std::endl;
            pendingIoCount = 0;
        }
    }
    // Wake up the loop to evaluate the exit condition
    uv_async_send(&asyncTaskHandle);
}

void EventLoop::run() {
    if (isRunning.exchange(true)) {
        throw std::runtime_error("EventLoop is already running on another thread or re-entrantly.");
    }

    struct RunGuard {
        std::atomic<bool>& flag;
        ~RunGuard() { flag = false; }
    } guard{isRunning};

    while (!stopRequested) {
        uv_run(uv_default_loop(), UV_RUN_DEFAULT);
        
        bool hasTasks = false;
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            hasTasks = !taskQueue.empty();
        }
        
        if (!hasTasks && pendingIoCount == 0) {
            break;
        }
    }
}

void EventLoop::stop() {
    stopRequested = true;
    uv_stop(uv_default_loop());
}
