#include "runtime/objects/EZObjects.h"
#include "gc/CycleCollector.h"
#include "runtime/RuntimeContext.h"
#include "runtime/Value.h"
#include <string>
#include <vector>
#include <memory>

void registerExceptionBuiltins(RuntimeContext& interp) {
    auto exceptionClass = std::make_shared<EZClass>("Exception");
    CycleCollector::instance().track(exceptionClass, ValueType::CLASS);
    exceptionClass->methods["init"] = Value::makeNativeFunction("init", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && args[0].isInstance()) {
                auto inst = args[0].asInstance();
                std::string message = args.size() > 1 ? args[1].toString() : "Unknown Error";
                inst->setProperty("message", Value(message));
            }
            return Value();
        });

    exceptionClass->methods["toString"] = Value::makeNativeFunction("toString", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && args[0].isInstance()) {
                auto inst = args[0].asInstance();
                Value msg = inst->getProperty("message");
                return Value(msg.isNil() ? std::string("Unknown Error") : msg.toString());
            }
            return Value(std::string("Unknown Error"));
        });
    interp.defineGlobal("Exception", Value(exceptionClass));

    auto makeErrorClass = [&](const std::string& name, std::shared_ptr<EZClass> parent) {
        auto cls = std::make_shared<EZClass>(name);
        CycleCollector::instance().track(cls, ValueType::CLASS);
        cls->parent = parent;
        interp.defineGlobal(name, Value(cls));
        return cls;
    };

    makeErrorClass("FileNotFoundError", exceptionClass);
    makeErrorClass("NetworkError", exceptionClass);
    makeErrorClass("TypeError", exceptionClass);
    makeErrorClass("ValueError", exceptionClass);
    makeErrorClass("IndexError", exceptionClass);
    makeErrorClass("KeyError", exceptionClass);
    makeErrorClass("PermissionError", exceptionClass);
    makeErrorClass("SecurityError", exceptionClass);
    makeErrorClass("IOError", exceptionClass);
    makeErrorClass("SyntaxError", exceptionClass);
    makeErrorClass("RegexError", exceptionClass);

    auto arithmeticClass = makeErrorClass("ArithmeticError", exceptionClass);
    makeErrorClass("ZeroDivisionError", arithmeticClass);
    makeErrorClass("OverflowError", arithmeticClass);

    makeErrorClass("AttributeError", exceptionClass);
    makeErrorClass("NameError", exceptionClass);
    makeErrorClass("RecursionError", exceptionClass);
    makeErrorClass("NotImplementedError", exceptionClass);
    makeErrorClass("TimeoutError", exceptionClass);
    makeErrorClass("AssertionError", exceptionClass);
    makeErrorClass("RateLimitError", exceptionClass);
    makeErrorClass("ModuleNotFoundError", exceptionClass);
}
