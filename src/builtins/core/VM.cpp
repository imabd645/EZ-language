#include "runtime/RuntimeContext.h"
#include "runtime/Value.h"
#include <string>
#include <vector>

void registerVMBuiltins(RuntimeContext& interp) {
    interp.defineGlobal("__vm_get_max_recursion_depth", Value::makeNativeFunction("__vm_get_max_recursion_depth", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            return Value((long long)interp.getMaxRecursionDepth());
        }));

    interp.defineGlobal("__vm_set_max_recursion_depth", Value::makeNativeFunction("__vm_set_max_recursion_depth", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) {
                interp.runtimeError("__vm_set_max_recursion_depth() expects a number", 0, "");
                return Value();
            }
            long long depth = (long long)args[0].asNumber();
            if (depth < 1) depth = 1;
            interp.setMaxRecursionDepth((size_t)depth);
            return Value(true);
        }));

    interp.defineGlobal("__vm_current_depth", Value::makeNativeFunction("__vm_current_depth", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            return Value((long long)interp.getCallDepth());
        }));

    interp.defineGlobal("__vm_instruction_count", Value::makeNativeFunction("__vm_instruction_count", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            return Value((long long)interp.getInstructionCount());
        }));

    interp.defineGlobal("__vm_reset_instruction_count", Value::makeNativeFunction("__vm_reset_instruction_count", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            interp.resetInstructionCount();
            return Value(true);
        }));

    interp.defineGlobal("__vm_get_max_instructions", Value::makeNativeFunction("__vm_get_max_instructions", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            return Value(static_cast<double>(interp.getMaxInstructions()));
        }));

    interp.defineGlobal("__vm_set_max_instructions", Value::makeNativeFunction("__vm_set_max_instructions", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.empty() || !args[0].isNumber()) {
                interp.runtimeError("__vm_set_max_instructions expects a number", 0, "");
                return Value();
            }
            interp.setMaxInstructions(static_cast<uint64_t>(args[0].asNumber()));
            return Value(true);
        }));

    interp.defineGlobal("__stack_trace", Value::makeNativeFunction("__stack_trace", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            return Value::makeArray(interp.getStackTraceFrames());
        }));

    interp.defineGlobal("__engine_info", Value::makeNativeFunction("__engine_info", 0,
        [](RuntimeContext&, const std::vector<Value>&) -> Value {
            Value dictVal = Value::makeDictionary();
            auto dict = dictVal.asDictionaryPtr();
            dict->modifyMap([&](auto& m) {
                m["version"] = Value("5.3.0");
                m["engine"] = Value("EZ Bytecode VM");
                #if defined(_WIN32) || defined(_WIN64)
                                m["platform"] = Value("windows");
                #elif defined(__APPLE__)
                                m["platform"] = Value("macos");
                #elif defined(__linux__)
                                m["platform"] = Value("linux");
                #else
                                m["platform"] = Value("unknown");
                #endif
                #if defined(__x86_64__) || defined(_M_X64)
                                m["arch"] = Value("x86_64");
                #elif defined(__aarch64__) || defined(_M_ARM64)
                                m["arch"] = Value("arm64");
                #else
                                m["arch"] = Value("x86");
                #endif
                                m["pointer_size"] = Value(static_cast<double>(sizeof(void*)));
                                m["endianness"] = Value("little");
                #if defined(__clang__)
                                m["compiler"] = Value("Clang " + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__));
                #elif defined(__GNUC__)
                                m["compiler"] = Value("GCC " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__));
                #elif defined(_MSC_VER)
                                m["compiler"] = Value("MSVC " + std::to_string(_MSC_VER));
                #else
                                m["compiler"] = Value("unknown");
                #endif
                            });
                            return dictVal;
                        }));
                }
