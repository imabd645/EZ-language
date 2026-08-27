#include "TestRunner.h"
#include "runtime/RuntimeContext.h"
#include "runtime/Value.h"
#include <iostream>
#include <string>
#include <sstream>

bool g_isTestMode = false;
std::vector<std::pair<std::string, Value>> TestRunner::registeredTests;

void TestRunner::registerTestBuiltins(RuntimeContext& interp) {
    // ez_register_test(name: string, body: task)
    interp.defineGlobal("ez_register_test", Value::makeNativeFunction("ez_register_test", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) {
                interp.runtimeError("ez_register_test() expects string as first argument", 0, "");
                return Value();
            }
            if (!args[1].isClosure()) {
                interp.runtimeError("ez_register_test() expects closure as second argument", 0, "");
                return Value();
            }
            
            // Only save the test if we are actually running tests
            if (g_isTestMode) {
                registeredTests.push_back({args[0].asString(), args[1]});
            }
            return Value();
        }));

    // assert(condition: bool, message: string = "")
    interp.defineGlobal("assert", Value::makeNativeFunction("assert", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.empty()) {
                interp.runtimeError("assert() expects at least 1 argument", 0, "");
                return Value();
            }
            
            bool condition = args[0].isTruthy();
            if (!condition) {
                std::string msg = "Assertion failed";
                if (args.size() > 1 && args[1].isString()) {
                    msg += ": " + args[1].asString();
                }
                interp.throwException("AssertionError", msg, 0, "");
            }
            return Value();
        }));
}

void TestRunner::runAllTests(std::shared_ptr<BytecodeVM> vm) {
    if (registeredTests.empty()) {
        std::cout << "\n\033[33mNo tests found.\033[0m\n";
        return;
    }

    std::cout << "\n\033[1mRunning " << registeredTests.size() << " tests...\033[0m\n\n";

    int passed = 0;
    int failed = 0;

    for (const auto& test : registeredTests) {
        std::cout << "test " << test.first << " ... ";
        std::cout.flush();
        
        std::stringstream buffer;
        std::streambuf* oldCerr = std::cerr.rdbuf(buffer.rdbuf());
        
        try {
            // Execute the test closure
            vm->callFunction(test.second, {});
            
            std::cerr.rdbuf(oldCerr);
            std::cout << "\033[32m[PASS]\033[0m\n";
            passed++;
        } catch (const RuntimeError& e) {
            std::cerr.rdbuf(oldCerr);
            std::cout << "\033[31m[FAIL]\033[0m";
            std::cout << buffer.str();
            failed++;
        } catch (const std::exception& e) {
            std::cerr.rdbuf(oldCerr);
            std::cout << "\033[31m[FAIL]\033[0m\n";
            std::cout << buffer.str();
            std::cerr << "  " << e.what() << "\n";
            failed++;
        }
    }

    std::cout << "\n\033[1mTest Summary:\033[0m\n";
    if (failed == 0) {
        std::cout << "\033[32mAll " << passed << " tests passed!\033[0m\n";
    } else {
        std::cout << "\033[32m" << passed << " passed\033[0m, \033[31m" << failed << " failed\033[0m\n";
    }
}
