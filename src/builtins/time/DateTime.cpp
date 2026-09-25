#include "runtime/objects/EZObjects.h"
#include "gc/CycleCollector.h"
#include "builtins/Builtins.h"
#include "runtime/RuntimeContext.h"
#include "runtime/Value.h"
#include "vm/BytecodeVM.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <memory>

void registerDateTimeBuiltins(RuntimeContext& interp) {


    // ════════════════════════════════════════════════════════════════════════════
    //  DateTime class
    // ════════════════════════════════════════════════════════════════════════════
    //
    //  Usage:
    //    now = DateTime()              # current time
    //    d   = DateTime(2026, 7, 17)   # specific date
    //    d   = DateTime(2026, 7, 17, 14, 30, 0)  # date + time
    //    out d.year()
    //    out d.format("%Y-%m-%d %H:%M:%S")
    //    out d.timestamp()             # milliseconds since epoch

    auto dtClass = std::make_shared<EZClass>("DateTime");
    CycleCollector::instance().track(dtClass, ValueType::CLASS);

    // DateTime.init(...)  — variadic: 0, 3, or 6 args
    //   0 args: current time
    //   3 args: year, month, day
    //   6 args: year, month, day, hour, minute, second
    dtClass->setMethod("init", Value::makeNativeFunction("init", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            long long ms = 0;

            if (args.size() == 1) {
                // No extra args — current time
                auto now = std::chrono::system_clock::now();
                ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()).count();
            } else if (args.size() == 4 || args.size() == 7) {
                // 3 or 6 args (+ the implicit 'self')
                int year   = static_cast<int>(args[1].asNumber());
                int month  = static_cast<int>(args[2].asNumber());
                int day    = static_cast<int>(args[3].asNumber());
                int hour   = (args.size() >= 7) ? static_cast<int>(args[4].asNumber()) : 0;
                int minute = (args.size() >= 7) ? static_cast<int>(args[5].asNumber()) : 0;
                int second = (args.size() >= 7) ? static_cast<int>(args[6].asNumber()) : 0;

                std::tm t{};
                t.tm_year  = year - 1900;
                t.tm_mon   = month - 1;
                t.tm_mday  = day;
                t.tm_hour  = hour;
                t.tm_min   = minute;
                t.tm_sec   = second;
                t.tm_isdst = -1;

                std::time_t epoch = std::mktime(&t);
                if (epoch == -1) {
                    interp.throwException("ValueError", "Invalid date/time values", 0, "");
                    return Value();
                }
                ms = static_cast<long long>(epoch) * 1000LL;
            } else {
                interp.throwException("TypeError",
                    "DateTime() expects 0, 3 (year,month,day), or 6 (year,month,day,hour,min,sec) arguments", 0, "");
                return Value();
            }

            instance->setProperty("_ms", Value(ms));
            return args[0];
        }));

    // DateTime.year() -> integer
    dtClass->setMethod("year", Value::makeNativeFunction("year", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            long long ms = static_cast<long long>(instance->getProperty("_ms").asNumber());
            std::time_t t = static_cast<std::time_t>(ms / 1000);
            std::tm* tm = std::localtime(&t);
            return Value(static_cast<long long>(tm->tm_year + 1900));
        }));

    // DateTime.month() -> integer (1-12)
    dtClass->setMethod("month", Value::makeNativeFunction("month", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            long long ms = static_cast<long long>(instance->getProperty("_ms").asNumber());
            std::time_t t = static_cast<std::time_t>(ms / 1000);
            std::tm* tm = std::localtime(&t);
            return Value(static_cast<long long>(tm->tm_mon + 1));
        }));

    // DateTime.day() -> integer (1-31)
    dtClass->setMethod("day", Value::makeNativeFunction("day", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            long long ms = static_cast<long long>(instance->getProperty("_ms").asNumber());
            std::time_t t = static_cast<std::time_t>(ms / 1000);
            std::tm* tm = std::localtime(&t);
            return Value(static_cast<long long>(tm->tm_mday));
        }));

    // DateTime.hour() -> integer (0-23)
    dtClass->setMethod("hour", Value::makeNativeFunction("hour", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            long long ms = static_cast<long long>(instance->getProperty("_ms").asNumber());
            std::time_t t = static_cast<std::time_t>(ms / 1000);
            std::tm* tm = std::localtime(&t);
            return Value(static_cast<long long>(tm->tm_hour));
        }));

    // DateTime.minute() -> integer (0-59)
    dtClass->setMethod("minute", Value::makeNativeFunction("minute", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            long long ms = static_cast<long long>(instance->getProperty("_ms").asNumber());
            std::time_t t = static_cast<std::time_t>(ms / 1000);
            std::tm* tm = std::localtime(&t);
            return Value(static_cast<long long>(tm->tm_min));
        }));

    // DateTime.second() -> integer (0-59)
    dtClass->setMethod("second", Value::makeNativeFunction("second", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            long long ms = static_cast<long long>(instance->getProperty("_ms").asNumber());
            std::time_t t = static_cast<std::time_t>(ms / 1000);
            std::tm* tm = std::localtime(&t);
            return Value(static_cast<long long>(tm->tm_sec));
        }));

    // DateTime.weekday() -> integer (0=Sunday, 6=Saturday)
    dtClass->setMethod("weekday", Value::makeNativeFunction("weekday", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            long long ms = static_cast<long long>(instance->getProperty("_ms").asNumber());
            std::time_t t = static_cast<std::time_t>(ms / 1000);
            std::tm* tm = std::localtime(&t);
            return Value(static_cast<long long>(tm->tm_wday));
        }));

    // DateTime.timestamp() -> integer (milliseconds since epoch)
    dtClass->setMethod("timestamp", Value::makeNativeFunction("timestamp", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            return instance->getProperty("_ms");
        }));

    // DateTime.format(fmt) -> string
    // Uses strftime format specifiers: %Y, %m, %d, %H, %M, %S, etc.
    dtClass->setMethod("format", Value::makeNativeFunction("format", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            if (!args[1].isString()) {
                interp.throwException("TypeError", "DateTime.format() expects string format", 0, "");
                return Value();
            }
            long long ms = static_cast<long long>(instance->getProperty("_ms").asNumber());
            std::time_t t = static_cast<std::time_t>(ms / 1000);
            std::tm* tm = std::localtime(&t);

            std::string fmt = args[1].asString();
            char buf[256];
            std::strftime(buf, sizeof(buf), fmt.c_str(), tm);
            return Value(std::string(buf));
        }));

    // DateTime.diff(other) -> integer (milliseconds between two DateTimes)
    dtClass->setMethod("diff", Value::makeNativeFunction("diff", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            if (!args[1].isInstance()) {
                interp.throwException("TypeError", "DateTime.diff() expects another DateTime", 0, "");
                return Value();
            }
            auto other = args[1].asInstance();
            long long ms1 = static_cast<long long>(instance->getProperty("_ms").asNumber());
            long long ms2 = static_cast<long long>(other->getProperty("_ms").asNumber());
            return Value(ms1 - ms2);
        }));

    // DateTime.addMs(ms) -> new DateTime
    dtClass->setMethod("addMs", Value::makeNativeFunction("addMs", 1,
        [dtClass](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            if (!args[1].isNumber()) {
                interp.throwException("TypeError", "DateTime.addMs() expects number", 0, "");
                return Value();
            }
            long long ms = static_cast<long long>(instance->getProperty("_ms").asNumber());
            long long delta = static_cast<long long>(args[1].asNumber());

            auto newInst = std::make_shared<EZInstance>(dtClass);
            CycleCollector::instance().track(newInst, ValueType::INSTANCE);
            newInst->setProperty("_ms", Value(ms + delta));
            return Value(newInst);
        }));

    // DateTime.addSeconds(n) -> new DateTime
    dtClass->setMethod("addSeconds", Value::makeNativeFunction("addSeconds", 1,
        [dtClass](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            if (!args[1].isNumber()) {
                interp.throwException("TypeError", "DateTime.addSeconds() expects number", 0, "");
                return Value();
            }
            long long ms = static_cast<long long>(instance->getProperty("_ms").asNumber());
            long long delta = static_cast<long long>(args[1].asNumber()) * 1000LL;

            auto newInst = std::make_shared<EZInstance>(dtClass);
            CycleCollector::instance().track(newInst, ValueType::INSTANCE);
            newInst->setProperty("_ms", Value(ms + delta));
            return Value(newInst);
        }));

    // DateTime.addDays(n) -> new DateTime
    dtClass->setMethod("addDays", Value::makeNativeFunction("addDays", 1,
        [dtClass](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            if (!args[1].isNumber()) {
                interp.throwException("TypeError", "DateTime.addDays() expects number", 0, "");
                return Value();
            }
            long long ms = static_cast<long long>(instance->getProperty("_ms").asNumber());
            long long delta = static_cast<long long>(args[1].asNumber()) * 86400000LL;

            auto newInst = std::make_shared<EZInstance>(dtClass);
            CycleCollector::instance().track(newInst, ValueType::INSTANCE);
            newInst->setProperty("_ms", Value(ms + delta));
            return Value(newInst);
        }));

    // DateTime.toString() -> string  (ISO 8601)
    dtClass->setMethod("toString", Value::makeNativeFunction("toString", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            long long ms = static_cast<long long>(instance->getProperty("_ms").asNumber());
            std::time_t t = static_cast<std::time_t>(ms / 1000);
            std::tm* tm = std::localtime(&t);
            char buf[64];
            std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", tm);
            return Value(std::string(buf));
        }));

    interp.defineGlobal("DateTime", Value(dtClass));


    // ════════════════════════════════════════════════════════════════════════════
    //  Timer class
    // ════════════════════════════════════════════════════════════════════════════
    //
    //  Usage:
    //    # One-shot timer (fires once after 2 seconds)
    //    t = Timer(2000)
    //    t.onTick(|| { out "done!" })
    //    t.start()
    //
    //    # Repeating timer
    //    t = Timer(1000, true)    # repeat = true
    //    t.onTick(|| { out "tick!" })
    //    t.start()
    //    wait(5000)
    //    t.stop()

    }
