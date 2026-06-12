#include "EventLoop.h"
#include <iostream>
#include "../Environment.h"
#define EZFUTURE_IMPL
#include "../EZFuture.h"

void EventLoop::pushTask(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        taskQueue.push(std::move(task));
    }
    cv.notify_one();
}

void EventLoop::retain() {
    std::lock_guard<std::mutex> lock(queueMutex);
    pendingIoCount++;
}

void EventLoop::release() {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        pendingIoCount--;
    }
    cv.notify_one();
}

void EventLoop::run() {
    while (true) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            
            // Wait until there is a task, OR there are no pending IOs (meaning we should exit)
            cv.wait(lock, [this]() {
                return !taskQueue.empty() || pendingIoCount == 0;
            });
            
            if (taskQueue.empty() && pendingIoCount == 0) {
                // Nothing left to do, exit the event loop
                break;
            }
            
            if (!taskQueue.empty()) {
                task = std::move(taskQueue.front());
                taskQueue.pop();
            }
        }
        
        // Execute task outside the lock
        if (task) {
            try {
                task();
            } catch (const std::exception& e) {
                std::cerr << "[EventLoop] Uncaught exception: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[EventLoop] Uncaught unknown exception" << std::endl;
            }
        }
    }
}
