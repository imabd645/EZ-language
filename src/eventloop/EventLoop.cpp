#include "eventloop/EventLoop.h"
#include <iostream>
#include "runtime/Environment.h"
#define EZFUTURE_IMPL
#include "runtime/EZFuture.h"

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
        if (pendingIoCount < 0) {
            std::cerr << "[EventLoop] WARNING: pendingIoCount underflow (double release detected)." << std::endl;
            pendingIoCount = 0;
        }
    }
    cv.notify_one();
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
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            
            // Wait until there is a task, OR there are no pending IOs (meaning we should exit), OR we are requested to stop
            cv.wait(lock, [this]() {
                return !taskQueue.empty() || pendingIoCount == 0 || stopRequested;
            });
            
            if (stopRequested || (taskQueue.empty() && pendingIoCount == 0)) {
                // Nothing left to do or stop requested, exit the event loop
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
            } catch (const RuntimeError& e) {
                // EZ Runtime errors
                std::cerr << "[EventLoop] Uncaught RuntimeError: " << e.what() << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[EventLoop] Uncaught std::exception: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[EventLoop] Uncaught unknown exception" << std::endl;
            }
        }
    }
}
