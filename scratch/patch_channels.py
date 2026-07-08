import re

with open("src/builtins/Builtins_Concurrency.cpp", "r") as f:
    content = f.read()

# Add includes and struct EZChannel
header_inject = """#include "builtins/Builtins.h"
#include "runtime/RuntimeContext.h"
#include "runtime/Value.h"
#include "runtime/Environment.h"
#include <thread>
#include <chrono>
#include <iostream>
#include <queue>
#include <condition_variable>
#include "eventloop/EventLoop.h"
#include "runtime/EZFuture.h"

struct EZChannel {
    std::queue<Value> q;
    std::mutex mtx;
    std::condition_variable cv;
    bool closed = false;
};

static std::mutex g_channelMapMutex;
static long long g_channelIdCounter = 1;
static std::unordered_map<long long, std::shared_ptr<EZChannel>> g_channels;
"""

content = re.sub(
    r'#include "builtins/Builtins\.h".*?#include "runtime/EZFuture\.h"',
    header_inject,
    content,
    flags=re.DOTALL
)

# Add Channel class registration
channel_class_inject = """
    // class Channel
    auto channelClass = std::make_shared<EZClass>("Channel");
    
    channelClass->methods["init"] = Value::makeNativeFunction("init", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            
            auto chan = std::make_shared<EZChannel>();
            long long id = 0;
            {
                std::lock_guard<std::mutex> guard(g_channelMapMutex);
                id = g_channelIdCounter++;
                g_channels[id] = chan;
            }
            
            instance->setProperty("_id", Value(id));
            return args[0];
        });
        
    channelClass->methods["send"] = Value::makeNativeFunction("send", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            long long id = instance->getProperty("_id").asInteger();
            
            std::shared_ptr<EZChannel> chan;
            {
                std::lock_guard<std::mutex> guard(g_channelMapMutex);
                auto it = g_channels.find(id);
                if (it == g_channels.end()) {
                    interp.runtimeError("Channel is closed or invalid", 0, "");
                    return Value();
                }
                chan = it->second;
            }
            
            {
                std::lock_guard<std::mutex> guard(chan->mtx);
                if (chan->closed) {
                    interp.runtimeError("Cannot send on closed channel", 0, "");
                    return Value();
                }
                chan->q.push(args[1]);
            }
            chan->cv.notify_one();
            
            return Value(true);
        });
        
    channelClass->methods["receive"] = Value::makeNativeFunction("receive", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            long long id = instance->getProperty("_id").asInteger();
            
            std::shared_ptr<EZChannel> chan;
            {
                std::lock_guard<std::mutex> guard(g_channelMapMutex);
                auto it = g_channels.find(id);
                if (it == g_channels.end()) {
                    interp.runtimeError("Channel is closed or invalid", 0, "");
                    return Value();
                }
                chan = it->second;
            }
            
            std::unique_lock<std::mutex> lock(chan->mtx);
            chan->cv.wait(lock, [&]() {
                return !chan->q.empty() || chan->closed;
            });
            
            if (!chan->q.empty()) {
                Value v = chan->q.front();
                chan->q.pop();
                return v;
            }
            
            return Value(); // Closed and empty
        });
        
    channelClass->methods["close"] = Value::makeNativeFunction("close", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            long long id = instance->getProperty("_id").asInteger();
            
            std::shared_ptr<EZChannel> chan;
            {
                std::lock_guard<std::mutex> guard(g_channelMapMutex);
                auto it = g_channels.find(id);
                if (it != g_channels.end()) {
                    chan = it->second;
                    g_channels.erase(it);
                }
            }
            
            if (chan) {
                std::lock_guard<std::mutex> guard(chan->mtx);
                chan->closed = true;
                chan->cv.notify_all();
            }
            
            return Value(true);
        });

    interp.defineGlobal("Channel", Value(channelClass));
"""

# Insert right before the closing brace of registerConcurrencyBuiltins
last_brace = content.rfind("}")
content = content[:last_brace] + channel_class_inject + content[last_brace:]

with open("src/builtins/Builtins_Concurrency.cpp", "w") as f:
    f.write(content)
