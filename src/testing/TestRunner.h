#ifndef TEST_RUNNER_H
#define TEST_RUNNER_H

#include "runtime/RuntimeContext.h"
#include "vm/BytecodeVM.h"
#include <vector>
#include <string>
#include <utility>
#include <memory>

// Global flag to indicate if we are in testing mode
extern bool g_isTestMode;

class TestRunner {
public:
    // Registers the builtins (ez_register_test and assert)
    static void registerTestBuiltins(RuntimeContext& interp);

    // Runs all registered tests
    static void runAllTests(std::shared_ptr<BytecodeVM> vm);

private:
    static std::vector<std::pair<std::string, Value>> registeredTests;
};

#endif // TEST_RUNNER_H
