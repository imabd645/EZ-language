#include "runtime/RuntimeContext.h"
#include "vm/BytecodeVM.h"
#include "runtime/EZFuture.h"
#include "builtins/Builtins.h"
#include <ffi.h>
#include <unordered_map>
#include <mutex>

struct CallbackClosure {
    ffi_closure* closure;
    ffi_cif cif;
    std::vector<ffi_type*> argTypes;
    Value ezFunction;
    RuntimeContext* interp;
    std::vector<std::string> sigTypes;
    std::string retType;
};

static std::unordered_map<void*, CallbackClosure*> g_callbacks;
static std::mutex g_callbacksMutex;

static void ffi_callback_dispatcher(ffi_cif* cif, void* ret, void** args, void* user_data) {
    CallbackClosure* cb = static_cast<CallbackClosure*>(user_data);
    if (!cb || !cb->interp) return;

    std::vector<Value> ezArgs;
    for (size_t i = 0; i < cb->sigTypes.size(); ++i) {
        const std::string& type = cb->sigTypes[i];
        if (type == "int" || type == "ptr" || type == "i64" || type == "u64") {
            long long val = *(long long*)args[i];
            ezArgs.push_back(Value(val));
        } else if (type == "i32" || type == "u32") {
            long long val = *(int32_t*)args[i];
            ezArgs.push_back(Value(val));
        } else if (type == "float" || type == "f32") {
            double val = *(float*)args[i];
            ezArgs.push_back(Value(val));
        } else if (type == "double" || type == "f64") {
            double val = *(double*)args[i];
            ezArgs.push_back(Value(val));
        } else if (type == "string") {
            const char* str = *(const char**)args[i];
            ezArgs.push_back(str ? Value(std::string(str)) : Value(""));
        } else {
            ezArgs.push_back(Value((long long)*(intptr_t*)args[i]));
        }
    }

    Value result = cb->interp->callFunction(cb->ezFunction, ezArgs, 0, "ffi_callback");

    if (cb->retType == "int" || cb->retType == "ptr" || cb->retType == "i64" || cb->retType == "u64") {
        *(long long*)ret = result.isNumber() ? (long long)result.asNumber() : 0;
    } else if (cb->retType == "i32" || cb->retType == "u32") {
        *(int32_t*)ret = result.isNumber() ? (int32_t)result.asNumber() : 0;
    } else if (cb->retType == "float" || cb->retType == "f32") {
        *(float*)ret = result.isNumber() ? (float)result.asFloat() : 0.0f;
    } else if (cb->retType == "double" || cb->retType == "f64") {
        *(double*)ret = result.isNumber() ? result.asFloat() : 0.0;
    } else if (cb->retType == "string") {
        *(const char**)ret = nullptr;
    } else if (cb->retType == "void") {
        // do nothing
    } else {
        *(intptr_t*)ret = result.isNumber() ? (intptr_t)result.asNumber() : 0;
    }
}

#include <iostream>
#include <chrono>
#include <thread>
#include <future>
#include <vector>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#define NOCOMM
#include <windows.h>
#include <setjmp.h>
#include <conio.h>
#include <shellapi.h>

// Include EZFuture (Windows-native future, must be after windows.h)

#include "runtime/EZFuture.h"

#ifdef _WIN32

#ifdef _MSC_VER
static intptr_t do_ffi_call(void* funcPtr, intptr_t* cArgs, size_t argc, bool isFloat, double& f_ret, bool& crashed) {
    intptr_t ret = 0;
    crashed = false;
    __try {
        if (isFloat) {
            using fFunc0 = double(*)(); using fFunc1 = double(*)(intptr_t); using fFunc2 = double(*)(intptr_t, intptr_t);
            using fFunc3 = double(*)(intptr_t, intptr_t, intptr_t); using fFunc4 = double(*)(intptr_t, intptr_t, intptr_t, intptr_t);
            if (argc == 0) f_ret = ((fFunc0)funcPtr)();
            else if (argc == 1) f_ret = ((fFunc1)funcPtr)(cArgs[0]);
            else if (argc == 2) f_ret = ((fFunc2)funcPtr)(cArgs[0], cArgs[1]);
            else if (argc == 3) f_ret = ((fFunc3)funcPtr)(cArgs[0], cArgs[1], cArgs[2]);
            else if (argc >= 4) f_ret = ((fFunc4)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3]);
        } else {
            using Func0 = intptr_t(*)(); using Func1 = intptr_t(*)(intptr_t); using Func2 = intptr_t(*)(intptr_t, intptr_t);
            using Func3 = intptr_t(*)(intptr_t, intptr_t, intptr_t); using Func4 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t);
            using Func5 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t); using Func6 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t);
            using Func7 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t); using Func8 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t);
            using Func9 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t); using Func10 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t);
            using Func11 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t); using Func12 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t);
            if (argc == 0) ret = ((Func0)funcPtr)();
            else if (argc == 1) ret = ((Func1)funcPtr)(cArgs[0]);
            else if (argc == 2) ret = ((Func2)funcPtr)(cArgs[0], cArgs[1]);
            else if (argc == 3) ret = ((Func3)funcPtr)(cArgs[0], cArgs[1], cArgs[2]);
            else if (argc == 4) ret = ((Func4)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3]);
            else if (argc == 5) ret = ((Func5)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4]);
            else if (argc == 6) ret = ((Func6)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5]);
            else if (argc == 7) ret = ((Func7)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6]);
            else if (argc == 8) ret = ((Func8)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7]);
            else if (argc == 9) ret = ((Func9)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7], cArgs[8]);
            else if (argc == 10) ret = ((Func10)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7], cArgs[8], cArgs[9]);
            else if (argc == 11) ret = ((Func11)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7], cArgs[8], cArgs[9], cArgs[10]);
            else if (argc >= 12) ret = ((Func12)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7], cArgs[8], cArgs[9], cArgs[10], cArgs[11]);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        crashed = true;
    }
    return ret;
}
#else
static jmp_buf os_call_jmp_env;
static LONG CALLBACK FfiVectoredHandler(PEXCEPTION_POINTERS ExceptionInfo) {
    DWORD code = ExceptionInfo->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_ACCESS_VIOLATION ||
        code == EXCEPTION_ILLEGAL_INSTRUCTION ||
        code == EXCEPTION_INT_DIVIDE_BY_ZERO ||
        code == EXCEPTION_PRIV_INSTRUCTION) {
        longjmp(os_call_jmp_env, 1);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static intptr_t do_ffi_call(void* funcPtr, intptr_t* cArgs, size_t argc, bool isFloat, double& f_ret, bool& crashed) {
    intptr_t ret = 0;
    crashed = false;
    PVOID vehHandler = AddVectoredExceptionHandler(1, FfiVectoredHandler);
    if (setjmp(os_call_jmp_env) == 0) {
        if (isFloat) {
            using fFunc0 = double(*)(); using fFunc1 = double(*)(intptr_t); using fFunc2 = double(*)(intptr_t, intptr_t);
            using fFunc3 = double(*)(intptr_t, intptr_t, intptr_t); using fFunc4 = double(*)(intptr_t, intptr_t, intptr_t, intptr_t);
            if (argc == 0) f_ret = ((fFunc0)funcPtr)();
            else if (argc == 1) f_ret = ((fFunc1)funcPtr)(cArgs[0]);
            else if (argc == 2) f_ret = ((fFunc2)funcPtr)(cArgs[0], cArgs[1]);
            else if (argc == 3) f_ret = ((fFunc3)funcPtr)(cArgs[0], cArgs[1], cArgs[2]);
            else if (argc >= 4) f_ret = ((fFunc4)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3]);
        } else {
            using Func0 = intptr_t(*)(); using Func1 = intptr_t(*)(intptr_t); using Func2 = intptr_t(*)(intptr_t, intptr_t);
            using Func3 = intptr_t(*)(intptr_t, intptr_t, intptr_t); using Func4 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t);
            using Func5 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t); using Func6 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t);
            using Func7 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t); using Func8 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t);
            using Func9 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t); using Func10 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t);
            using Func11 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t); using Func12 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t);
            if (argc == 0) ret = ((Func0)funcPtr)();
            else if (argc == 1) ret = ((Func1)funcPtr)(cArgs[0]);
            else if (argc == 2) ret = ((Func2)funcPtr)(cArgs[0], cArgs[1]);
            else if (argc == 3) ret = ((Func3)funcPtr)(cArgs[0], cArgs[1], cArgs[2]);
            else if (argc == 4) ret = ((Func4)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3]);
            else if (argc == 5) ret = ((Func5)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4]);
            else if (argc == 6) ret = ((Func6)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5]);
            else if (argc == 7) ret = ((Func7)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6]);
            else if (argc == 8) ret = ((Func8)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7]);
            else if (argc == 9) ret = ((Func9)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7], cArgs[8]);
            else if (argc == 10) ret = ((Func10)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7], cArgs[8], cArgs[9]);
            else if (argc == 11) ret = ((Func11)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7], cArgs[8], cArgs[9], cArgs[10]);
            else if (argc >= 12) ret = ((Func12)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7], cArgs[8], cArgs[9], cArgs[10], cArgs[11]);
        }
    } else {
        crashed = true;
    }
    RemoveVectoredExceptionHandler(vehHandler);
    return ret;
}
#endif
#endif

static LRESULT CALLBACK EZ_ProxyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Only redirect SENT messages (WM_COMMAND, WM_NOTIFY)
    // Redirect with 0x8000 offset to avoid infinite loop when DispatchMessage calls this proxy
    if (msg == WM_COMMAND || msg == WM_NOTIFY) {
        PostMessage(hwnd, msg + 0x8000, wParam, lParam);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
#endif

struct StructField {
    size_t size;
    size_t align;
    size_t offset;
    std::string type;
};

struct StructLayout {
    std::vector<StructField> fields;
    size_t totalSize;
    size_t maxAlign;
};

static bool parseStructType(const std::string& type, size_t& size, size_t& align) {
    if (type == "i8" || type == "u8") { size = 1; align = 1; return true; }
    if (type == "i16" || type == "u16") { size = 2; align = 2; return true; }
    if (type == "i32" || type == "u32" || type == "f32") { size = 4; align = 4; return true; }
    if (type == "i64" || type == "u64" || type == "f64" || type == "ptr") { size = 8; align = 8; return true; }
    return false;
}

static StructLayout computeStructLayout(const std::vector<Value>& layoutArray) {
    StructLayout layout;
    layout.totalSize = 0;
    layout.maxAlign = 1;

    for (const auto& val : layoutArray) {
        if (!val.isString()) continue;
        std::string type = val.asString();
        size_t size = 0, align = 0;
        if (!parseStructType(type, size, align)) {
            size = 1; align = 1; // Fallback
        }

        if (align > layout.maxAlign) layout.maxAlign = align;
        
        layout.totalSize = (layout.totalSize + align - 1) & ~(align - 1);
        
        StructField field;
        field.size = size;
        field.align = align;
        field.offset = layout.totalSize;
        field.type = type;
        layout.fields.push_back(field);
        
        layout.totalSize += size;
    }

    layout.totalSize = (layout.totalSize + layout.maxAlign - 1) & ~(layout.maxAlign - 1);
    return layout;
}

void registerFFIBuiltins(RuntimeContext& interp) {
    interp.defineGlobal("os_alloc", Value::makeNativeFunction("os_alloc", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber()) return Value();
            size_t size = (size_t)args[0].asNumber();
            void* ptr = calloc(1, size);
            return Value((long long)(reinterpret_cast<uintptr_t>(ptr)));
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_free", Value::makeNativeFunction("os_free", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber()) return Value();
            void* ptr = reinterpret_cast<void*>((uintptr_t)args[0].asNumber());
            if (ptr) free(ptr);
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_read_uint64", Value::makeNativeFunction("os_read_uint64", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0LL);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            uint64_t val = *(uint64_t*)(base + offset);
            return Value((long long)val);
#else
            return Value(0LL);
#endif
        }));

    interp.defineGlobal("os_write_uint64", Value::makeNativeFunction("os_write_uint64", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            *(uint64_t*)(base + offset) = (uint64_t)args[2].asNumber();
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_read_int64", Value::makeNativeFunction("os_read_int64", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0LL);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            int64_t val = *(int64_t*)(base + offset);
            return Value((long long)val);
#else
            return Value(0LL);
#endif
        }));

    interp.defineGlobal("os_write_int64", Value::makeNativeFunction("os_write_int64", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            *(int64_t*)(base + offset) = (int64_t)args[2].asNumber();
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_read_uint32", Value::makeNativeFunction("os_read_uint32", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0LL);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            uint32_t val = *(uint32_t*)(base + offset);
            return Value((long long)val);
#else
            return Value(0LL);
#endif
        }));

    interp.defineGlobal("os_read_int32", Value::makeNativeFunction("os_read_int32", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0LL);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            int32_t val = *(int32_t*)(base + offset);
            return Value((long long)val);
#else
            return Value(0LL);
#endif
        }));

    interp.defineGlobal("os_write_uint32", Value::makeNativeFunction("os_write_uint32", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            *(uint32_t*)(base + offset) = (uint32_t)args[2].asNumber();
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_write_int32", Value::makeNativeFunction("os_write_int32", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            *(int32_t*)(base + offset) = (int32_t)args[2].asNumber();
            return Value();
#else
            return Value();
#endif
        }));



    interp.defineGlobal("os_get_proxy_wndproc", Value::makeNativeFunction("os_get_proxy_wndproc", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            return Value((long long)(uintptr_t)EZ_ProxyWndProc);
#else
            return Value(0LL);
#endif
        }));

    interp.defineGlobal("os_read_uint16", Value::makeNativeFunction("os_read_uint16", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0LL);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            uint16_t val = *(uint16_t*)(base + offset);
            return Value((long long)val);
#else
            return Value(0LL);
#endif
        }));

    interp.defineGlobal("os_read_int16", Value::makeNativeFunction("os_read_int16", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0LL);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            int16_t val = *(int16_t*)(base + offset);
            return Value((long long)val);
#else
            return Value(0LL);
#endif
        }));

    interp.defineGlobal("os_write_uint16", Value::makeNativeFunction("os_write_uint16", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            *(uint16_t*)(base + offset) = (uint16_t)args[2].asNumber();
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_write_int16", Value::makeNativeFunction("os_write_int16", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            *(int16_t*)(base + offset) = (int16_t)args[2].asNumber();
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_write_uint64", Value::makeNativeFunction("os_write_uint64", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            *(uint64_t*)(base + offset) = (uint64_t)args[2].asNumber();
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_read_float32", Value::makeNativeFunction("os_read_float32", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0.0);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            float val = *(float*)(base + offset);
            return Value((double)val);
#else
            return Value(0.0);
#endif
        }));

    interp.defineGlobal("os_write_float32", Value::makeNativeFunction("os_write_float32", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            *(float*)(base + offset) = (float)args[2].asFloat();
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_read_float64", Value::makeNativeFunction("os_read_float64", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0.0);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            double val = *(double*)(base + offset);
            return Value(val);
#else
            return Value(0.0);
#endif
        }));

    interp.defineGlobal("os_write_float64", Value::makeNativeFunction("os_write_float64", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            *(double*)(base + offset) = args[2].asFloat();
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_read_float", Value::makeNativeFunction("os_read_float", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0.0);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            double val = *(double*)(base + offset);
            return Value(val);
#else
            return Value(0.0);
#endif
        }));

    interp.defineGlobal("os_write_float", Value::makeNativeFunction("os_write_float", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            *(double*)(base + offset) = args[2].asFloat();
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_read_double", Value::makeNativeFunction("os_read_double", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0.0);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            double val = *(double*)(base + offset);
            return Value(val);
#else
            return Value(0.0);
#endif
        }));

    interp.defineGlobal("os_write_double", Value::makeNativeFunction("os_write_double", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            *(double*)(base + offset) = args[2].asFloat();
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_read_byte", Value::makeNativeFunction("os_read_byte", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0LL);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            return Value((long long)*(base + offset));
#else
            return Value(0LL);
#endif
        }));

    interp.defineGlobal("os_write_byte", Value::makeNativeFunction("os_write_byte", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            *(base + offset) = (uint8_t)args[2].asNumber();
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_read_string_ptr", Value::makeNativeFunction("os_read_string_ptr", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber()) return Value("");
            const char* str = reinterpret_cast<const char*>((uintptr_t)args[0].asNumber());
            if (str) return Value(std::string(str));
            return Value("");
#else
            return Value("");
#endif
        }));

    interp.defineGlobal("os_write_string", Value::makeNativeFunction("os_write_string", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isString()) return Value();
            char* base = reinterpret_cast<char*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            std::string text = args[2].asString();
            memcpy(base + offset, text.c_str(), text.length() + 1);
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_load_lib", Value::makeNativeFunction("os_load_lib", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isString()) return Value();
            HMODULE handle = LoadLibraryA(args[0].asString().c_str());
            return Value((long long)(reinterpret_cast<uintptr_t>(handle)));
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_get_func", Value::makeNativeFunction("os_get_func", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isString()) return Value();
            HMODULE handle = reinterpret_cast<HMODULE>((uintptr_t)args[0].asNumber());
            FARPROC proc = GetProcAddress(handle, args[1].asString().c_str());
            return Value((long long)(reinterpret_cast<uintptr_t>(proc)));
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_call", Value::makeNativeFunction("os_call", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (args.size() < 2 || !args[0].isNumber() || !args[1].isString()) return Value();
            void* funcPtr = reinterpret_cast<void*>((uintptr_t)args[0].asNumber());
            if (!funcPtr) {
                interp.runtimeError("os_call: Null function pointer or function not found.", 0, "");
                return Value();
            }
            
            std::string tempStrings[12];
            intptr_t cArgs[12] = {0};
            for (size_t i = 2; i < args.size() && i - 2 < 12; i++) {
                size_t idx = i - 2;
                if (args[i].isNumber()) cArgs[idx] = static_cast<intptr_t>(args[i].asNumber());
                else if (args[i].isString()) {
                    tempStrings[idx] = args[i].asString();
                    cArgs[idx] = reinterpret_cast<intptr_t>(tempStrings[idx].c_str());
                }
                else if (args[i].isBuffer()) cArgs[idx] = reinterpret_cast<intptr_t>(args[i].asBuffer().data());
                else if (args[i].isBool()) cArgs[idx] = args[i].asBool() ? 1 : 0;
            }
            
            size_t argc = args.size() - 2;
            std::string retType = args[1].asString();

            bool crashed = false;
            double f_ret = 0.0;
            intptr_t ret = do_ffi_call(funcPtr, cArgs, argc, (retType == "float"), f_ret, crashed);

            if (crashed) {
                interp.runtimeError("os_call: Access violation or fatal memory fault inside external DLL.", 0, "");
                return Value();
            }
            
            if (retType == "float") return Value(f_ret);
            
            if (retType == "int" || retType == "ptr") return Value((long long)ret);
            if (retType == "string") {
                const char* str = reinterpret_cast<const char*>(ret);
                if (str) return Value(std::string(str));
                return Value("");
            }
            return Value();
#else
            return Value();
#endif
        }));

#ifdef _WIN32
#define FFI_DISPATCH_4(retT) \
    switch(floatMask) { \
        case 0: { using F = retT(*)(intptr_t, intptr_t, intptr_t, intptr_t); \
            if constexpr(std::is_same_v<retT,double>) f_ret = ((F)funcPtr)(iArgs[0], iArgs[1], iArgs[2], iArgs[3]); \
            else i_ret = ((F)funcPtr)(iArgs[0], iArgs[1], iArgs[2], iArgs[3]); break; } \
        case 1: { using F = retT(*)(double, intptr_t, intptr_t, intptr_t); \
            if constexpr(std::is_same_v<retT,double>) f_ret = ((F)funcPtr)(fArgs[0], iArgs[1], iArgs[2], iArgs[3]); \
            else i_ret = ((F)funcPtr)(fArgs[0], iArgs[1], iArgs[2], iArgs[3]); break; } \
        case 2: { using F = retT(*)(intptr_t, double, intptr_t, intptr_t); \
            if constexpr(std::is_same_v<retT,double>) f_ret = ((F)funcPtr)(iArgs[0], fArgs[1], iArgs[2], iArgs[3]); \
            else i_ret = ((F)funcPtr)(iArgs[0], fArgs[1], iArgs[2], iArgs[3]); break; } \
        case 3: { using F = retT(*)(double, double, intptr_t, intptr_t); \
            if constexpr(std::is_same_v<retT,double>) f_ret = ((F)funcPtr)(fArgs[0], fArgs[1], iArgs[2], iArgs[3]); \
            else i_ret = ((F)funcPtr)(fArgs[0], fArgs[1], iArgs[2], iArgs[3]); break; } \
        case 4: { using F = retT(*)(intptr_t, intptr_t, double, intptr_t); \
            if constexpr(std::is_same_v<retT,double>) f_ret = ((F)funcPtr)(iArgs[0], iArgs[1], fArgs[2], iArgs[3]); \
            else i_ret = ((F)funcPtr)(iArgs[0], iArgs[1], fArgs[2], iArgs[3]); break; } \
        case 5: { using F = retT(*)(double, intptr_t, double, intptr_t); \
            if constexpr(std::is_same_v<retT,double>) f_ret = ((F)funcPtr)(fArgs[0], iArgs[1], fArgs[2], iArgs[3]); \
            else i_ret = ((F)funcPtr)(fArgs[0], iArgs[1], fArgs[2], iArgs[3]); break; } \
        case 6: { using F = retT(*)(intptr_t, double, double, intptr_t); \
            if constexpr(std::is_same_v<retT,double>) f_ret = ((F)funcPtr)(iArgs[0], fArgs[1], fArgs[2], iArgs[3]); \
            else i_ret = ((F)funcPtr)(iArgs[0], fArgs[1], fArgs[2], iArgs[3]); break; } \
        case 7: { using F = retT(*)(double, double, double, intptr_t); \
            if constexpr(std::is_same_v<retT,double>) f_ret = ((F)funcPtr)(fArgs[0], fArgs[1], fArgs[2], iArgs[3]); \
            else i_ret = ((F)funcPtr)(fArgs[0], fArgs[1], fArgs[2], iArgs[3]); break; } \
        case 8: { using F = retT(*)(intptr_t, intptr_t, intptr_t, double); \
            if constexpr(std::is_same_v<retT,double>) f_ret = ((F)funcPtr)(iArgs[0], iArgs[1], iArgs[2], fArgs[3]); \
            else i_ret = ((F)funcPtr)(iArgs[0], iArgs[1], iArgs[2], fArgs[3]); break; } \
        case 9: { using F = retT(*)(double, intptr_t, intptr_t, double); \
            if constexpr(std::is_same_v<retT,double>) f_ret = ((F)funcPtr)(fArgs[0], iArgs[1], iArgs[2], fArgs[3]); \
            else i_ret = ((F)funcPtr)(fArgs[0], iArgs[1], iArgs[2], fArgs[3]); break; } \
        case 10: { using F = retT(*)(intptr_t, double, intptr_t, double); \
            if constexpr(std::is_same_v<retT,double>) f_ret = ((F)funcPtr)(iArgs[0], fArgs[1], iArgs[2], fArgs[3]); \
            else i_ret = ((F)funcPtr)(iArgs[0], fArgs[1], iArgs[2], fArgs[3]); break; } \
        case 11: { using F = retT(*)(double, double, intptr_t, double); \
            if constexpr(std::is_same_v<retT,double>) f_ret = ((F)funcPtr)(fArgs[0], fArgs[1], iArgs[2], fArgs[3]); \
            else i_ret = ((F)funcPtr)(fArgs[0], fArgs[1], iArgs[2], fArgs[3]); break; } \
        case 12: { using F = retT(*)(intptr_t, intptr_t, double, double); \
            if constexpr(std::is_same_v<retT,double>) f_ret = ((F)funcPtr)(iArgs[0], iArgs[1], fArgs[2], fArgs[3]); \
            else i_ret = ((F)funcPtr)(iArgs[0], iArgs[1], fArgs[2], fArgs[3]); break; } \
        case 13: { using F = retT(*)(double, intptr_t, double, double); \
            if constexpr(std::is_same_v<retT,double>) f_ret = ((F)funcPtr)(fArgs[0], iArgs[1], fArgs[2], fArgs[3]); \
            else i_ret = ((F)funcPtr)(fArgs[0], iArgs[1], fArgs[2], fArgs[3]); break; } \
        case 14: { using F = retT(*)(intptr_t, double, double, double); \
            if constexpr(std::is_same_v<retT,double>) f_ret = ((F)funcPtr)(iArgs[0], fArgs[1], fArgs[2], fArgs[3]); \
            else i_ret = ((F)funcPtr)(iArgs[0], fArgs[1], fArgs[2], fArgs[3]); break; } \
        case 15: { using F = retT(*)(double, double, double, double); \
            if constexpr(std::is_same_v<retT,double>) f_ret = ((F)funcPtr)(fArgs[0], fArgs[1], fArgs[2], fArgs[3]); \
            else i_ret = ((F)funcPtr)(fArgs[0], fArgs[1], fArgs[2], fArgs[3]); break; } \
    }
#endif

    interp.defineGlobal("os_call_sig", Value::makeNativeFunction("os_call_sig", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (args.size() < 3 || !args[0].isNumber() || !args[1].isString() || !args[2].isArray()) return Value();
            void* funcPtr = reinterpret_cast<void*>((uintptr_t)args[0].asNumber());
            if (!funcPtr) {
                interp.runtimeError("os_call_sig: Null function pointer.", 0, "");
                return Value();
            }
            
            std::string retType = args[1].asString();
            auto sigArray = args[2].asArray().elements;
            size_t argc = sigArray.size();
            if (argc > 4) {
                interp.runtimeError("os_call_sig: Currently supports up to 4 arguments.", 0, "");
                return Value();
            }
            
            int floatMask = 0;
            for (size_t i = 0; i < argc; i++) {
                if (sigArray[i].isString() && (sigArray[i].asString() == "float" || sigArray[i].asString() == "double")) {
                    floatMask |= (1 << i);
                }
            }
            
            intptr_t iArgs[4] = {0};
            double fArgs[4] = {0.0};
            std::string tempStrings[4];
            
            for (size_t i = 0; i < argc; i++) {
                size_t valIdx = i + 3;
                if (valIdx >= args.size()) break;
                
                if (floatMask & (1 << i)) {
                    if (args[valIdx].isNumber()) fArgs[i] = args[valIdx].asNumber();
                } else {
                    if (args[valIdx].isNumber()) iArgs[i] = (intptr_t)args[valIdx].asNumber();
                    else if (args[valIdx].isString()) {
                        tempStrings[i] = args[valIdx].asString();
                        iArgs[i] = reinterpret_cast<intptr_t>(tempStrings[i].c_str());
                    } else if (args[valIdx].isBuffer()) {
                        iArgs[i] = reinterpret_cast<intptr_t>(args[valIdx].asBuffer().data());
                    } else if (args[valIdx].isBool()) {
                        iArgs[i] = args[valIdx].asBool() ? 1 : 0;
                    }
                }
            }

            bool crashed = false;
            double f_ret = 0.0;
            intptr_t i_ret = 0;
            
            PVOID vehHandler = nullptr;
#ifndef _MSC_VER
            vehHandler = AddVectoredExceptionHandler(1, FfiVectoredHandler);
#endif

#ifdef _MSC_VER
            __try {
#else
            if (setjmp(os_call_jmp_env) == 0) {
#endif
                if (retType == "float" || retType == "double") {
                    FFI_DISPATCH_4(double)
                } else {
                    FFI_DISPATCH_4(intptr_t)
                }
#ifdef _MSC_VER
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                crashed = true;
            }
#else
            } else {
                crashed = true;
            }
            RemoveVectoredExceptionHandler(vehHandler);
#endif

            if (crashed) {
                interp.runtimeError("os_call_sig: Access violation or fatal memory fault.", 0, "");
                return Value();
            }
            
            if (retType == "float" || retType == "double") return Value(f_ret);
            if (retType == "int" || retType == "ptr") return Value((long long)i_ret);
            if (retType == "string") {
                const char* str = reinterpret_cast<const char*>(i_ret);
                if (str) return Value(std::string(str));
                return Value("");
            }
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_struct_alloc", Value::makeNativeFunction("os_struct_alloc", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                interp.runtimeError("os_struct_alloc expects an array of strings", 0, "");
                return Value();
            }
            auto layoutArray = args[0].asArray().elements;
            StructLayout layout = computeStructLayout(layoutArray);
            return Value(std::make_shared<EZBuffer>(layout.totalSize));
        }));

    interp.defineGlobal("os_struct_pack", Value::makeNativeFunction("os_struct_pack", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.size() < 2 || !args[0].isArray() || !args[1].isArray()) {
                interp.runtimeError("os_struct_pack expects (layout_array, values_array, [buffer])", 0, "");
                return Value();
            }
            auto layoutArray = args[0].asArray().elements;
            auto valuesArray = args[1].asArray().elements;
            StructLayout layout = computeStructLayout(layoutArray);
            
            Value bufVal;
            if (args.size() >= 3 && args[2].isBuffer()) {
                bufVal = args[2];
                if (bufVal.asBuffer().size() < layout.totalSize) {
                    interp.runtimeError("os_struct_pack: buffer too small", 0, "");
                    return Value();
                }
            } else {
                bufVal = Value(std::make_shared<EZBuffer>(layout.totalSize));
            }
            
            uint8_t* base = bufVal.asBuffer().data();
            for (size_t i = 0; i < layout.fields.size() && i < valuesArray.size(); i++) {
                const auto& field = layout.fields[i];
                Value val = valuesArray[i];
                uint8_t* ptr = base + field.offset;
                
                if (field.type == "i8" || field.type == "u8") {
                    *ptr = (uint8_t)(val.isNumber() ? val.asNumber() : 0);
                } else if (field.type == "i16" || field.type == "u16") {
                    *(uint16_t*)ptr = (uint16_t)(val.isNumber() ? val.asNumber() : 0);
                } else if (field.type == "i32" || field.type == "u32") {
                    *(uint32_t*)ptr = (uint32_t)(val.isNumber() ? val.asNumber() : 0);
                } else if (field.type == "i64" || field.type == "u64" || field.type == "ptr") {
                    *(uint64_t*)ptr = (uint64_t)(val.isNumber() ? val.asNumber() : 0);
                } else if (field.type == "f32") {
                    *(float*)ptr = (float)(val.isNumber() ? val.asFloat() : 0.0f);
                } else if (field.type == "f64") {
                    *(double*)ptr = (double)(val.isNumber() ? val.asFloat() : 0.0);
                }
            }
            
            return bufVal;
        }));

    interp.defineGlobal("os_struct_unpack", Value::makeNativeFunction("os_struct_unpack", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                interp.runtimeError("os_struct_unpack expects (layout_array, buffer_or_ptr)", 0, "");
                return Value();
            }
            auto layoutArray = args[0].asArray().elements;
            StructLayout layout = computeStructLayout(layoutArray);
            
            uint8_t* base = nullptr;
            if (args[1].isBuffer()) {
                if (args[1].asBuffer().size() < layout.totalSize) {
                    interp.runtimeError("os_struct_unpack: buffer too small", 0, "");
                    return Value();
                }
                base = args[1].asBuffer().data();
            } else if (args[1].isNumber()) {
                base = reinterpret_cast<uint8_t*>((uintptr_t)args[1].asNumber());
            } else {
                interp.runtimeError("os_struct_unpack: expects buffer or pointer address", 0, "");
                return Value();
            }
            
            if (!base) return Value::makeArray({});

            std::vector<Value> results;
            for (size_t i = 0; i < layout.fields.size(); i++) {
                const auto& field = layout.fields[i];
                uint8_t* ptr = base + field.offset;
                
                if (field.type == "i8") {
                    results.push_back(Value((long long)*(int8_t*)ptr));
                } else if (field.type == "u8") {
                    results.push_back(Value((long long)*(uint8_t*)ptr));
                } else if (field.type == "i16") {
                    results.push_back(Value((long long)*(int16_t*)ptr));
                } else if (field.type == "u16") {
                    results.push_back(Value((long long)*(uint16_t*)ptr));
                } else if (field.type == "i32") {
                    results.push_back(Value((long long)*(int32_t*)ptr));
                } else if (field.type == "u32") {
                    results.push_back(Value((long long)*(uint32_t*)ptr));
                } else if (field.type == "i64" || field.type == "u64" || field.type == "ptr") {
                    results.push_back(Value((long long)*(uint64_t*)ptr));
                } else if (field.type == "f32") {
                    results.push_back(Value((double)*(float*)ptr));
                } else if (field.type == "f64") {
                    results.push_back(Value((double)*(double*)ptr));
                } else {
                    results.push_back(Value());
                }
            }
            
            return Value::makeArray(results);
        }));

    interp.defineGlobal("os_ffi_create_callback", Value::makeNativeFunction("os_ffi_create_callback", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) { interp.runtimeError("arg 0 must be array of strings", 0, ""); return Value(); }
            std::vector<std::string> sigTypes;
            for (auto& v : args[0].asArray().elements) sigTypes.push_back(v.toString());
            std::string retType = args[1].toString();
            Value ezFunction = args[2];

            CallbackClosure* cb = new CallbackClosure();
            cb->ezFunction = ezFunction;
            cb->interp = &interp;
            cb->sigTypes = sigTypes;
            cb->retType = retType;

            for(auto t : sigTypes) {
                if (t == "int" || t == "ptr" || t == "i64" || t == "u64") cb->argTypes.push_back(&ffi_type_sint64);
                else if (t == "i32" || t == "u32") cb->argTypes.push_back(&ffi_type_sint32);
                else if (t == "float" || t == "f32") cb->argTypes.push_back(&ffi_type_float);
                else if (t == "double" || t == "f64") cb->argTypes.push_back(&ffi_type_double);
                else if (t == "string") cb->argTypes.push_back(&ffi_type_pointer);
                else cb->argTypes.push_back(&ffi_type_pointer);
            }

            ffi_type* ffiRetType = &ffi_type_void;
            if (retType == "int" || retType == "ptr" || retType == "i64" || retType == "u64") ffiRetType = &ffi_type_sint64;
            else if (retType == "i32" || retType == "u32") ffiRetType = &ffi_type_sint32;
            else if (retType == "float" || retType == "f32") ffiRetType = &ffi_type_float;
            else if (retType == "double" || retType == "f64") ffiRetType = &ffi_type_double;
            else if (retType == "string") ffiRetType = &ffi_type_pointer;

            if (ffi_prep_cif(&cb->cif, FFI_DEFAULT_ABI, cb->argTypes.size(), ffiRetType, cb->argTypes.data()) != FFI_OK) {
                delete cb;
                interp.runtimeError("ffi_prep_cif failed", 0, "");
                return Value();
            }

            void* bound_ptr = nullptr;
            cb->closure = (ffi_closure*)ffi_closure_alloc(sizeof(ffi_closure), &bound_ptr);
            if (!cb->closure) {
                delete cb;
                interp.runtimeError("ffi_closure_alloc failed", 0, "");
                return Value();
            }

            if (ffi_prep_closure_loc(cb->closure, &cb->cif, ffi_callback_dispatcher, cb, bound_ptr) != FFI_OK) {
                ffi_closure_free(cb->closure);
                delete cb;
                interp.runtimeError("ffi_prep_closure_loc failed", 0, "");
                return Value();
            }

            {
                std::lock_guard<std::mutex> lock(g_callbacksMutex);
                g_callbacks[bound_ptr] = cb;
            }

            return Value((long long)(intptr_t)bound_ptr);
        }));

    interp.defineGlobal("os_ffi_free_callback", Value::makeNativeFunction("os_ffi_free_callback", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            void* ptr = (void*)(intptr_t)args[0].asNumber();
            std::lock_guard<std::mutex> lock(g_callbacksMutex);
            auto it = g_callbacks.find(ptr);
            if (it != g_callbacks.end()) {
                ffi_closure_free(it->second->closure);
                delete it->second;
                g_callbacks.erase(it);
            }
            return Value();
        }));
}

