#include "FFI_Internal.h"

// ============================================================================
// Native -> EZ callbacks. os_ffi_create_callback compiles an EZ function into a
// libffi closure so native code can call back into the interpreter, plus the
// Win32 window-procedure trampoline the GUI layer routes messages through.
//
// A callback stays alive until os_ffi_free_callback releases it.
// ============================================================================

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

// Capture the main thread ID when the application loads this translation unit
static std::thread::id g_mainThreadId = std::this_thread::get_id();

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

    Value result;
    if (std::this_thread::get_id() == g_mainThreadId) {
        result = cb->interp->callFunction(cb->ezFunction, ezArgs, 0, "ffi_callback");
    } else {
        std::promise<Value> p;
        auto f = p.get_future();
        EventLoop::instance().pushTask([cb, ezArgs, &p]() {
            try {
                p.set_value(cb->interp->callFunction(cb->ezFunction, ezArgs, 0, "ffi_callback"));
            } catch (...) {
                p.set_value(Value());
            }
        });
        result = f.get();
    }

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

#ifdef _WIN32
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

void registerFFICallback(RuntimeContext& interp) {
    interp.defineGlobal("os_get_proxy_wndproc", Value::makeNativeFunction("os_get_proxy_wndproc", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            return Value((long long)(uintptr_t)EZ_ProxyWndProc);
#else
            return Value(0LL);
#endif
        }));


    interp.defineGlobal("os_ffi_create_callback", Value::makeNativeFunction("os_ffi_create_callback", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) { interp.runtimeError("arg 0 must be array of strings", 0, ""); return Value(); }
            std::vector<std::string> sigTypes;
            for (auto& v : args[0].asArray().getElementsCopy()) sigTypes.push_back(v.toString());
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

    // ── Production-ready additions ────────────────────────────────────────────

    // Bounded C-string read: stops at NUL or maxlen bytes, whichever comes first.
    // Prevents runaway reads on un-terminated foreign strings.

    interp.defineGlobal("os_is_callback", Value::makeNativeFunction("os_is_callback", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) return Value(false);
            void* ptr = (void*)(intptr_t)args[0].asNumber();
            std::lock_guard<std::mutex> lock(g_callbacksMutex);
            return Value(g_callbacks.count(ptr) > 0);
        }));

    // os_call_sig_arr(funcPtr, retType, sigArray, argsArray)
    // Fully variadic os_call_sig variant: takes user args as an EZ array so
    // there is no N-argument limit at the EZ library level.
}
