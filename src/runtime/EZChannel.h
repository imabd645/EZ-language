#ifndef EZCHANNEL_H
#define EZCHANNEL_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include "runtime/Value.h"

struct EZChannel {
    std::queue<Value> q;
    std::mutex mtx;
    std::condition_variable cv;
    bool closed = false;
};

#endif // EZCHANNEL_H
