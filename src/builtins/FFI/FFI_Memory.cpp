#include "FFI_Internal.h"

// ============================================================================
// Raw memory: allocation and every fixed-width read/write.
//
// Signed and unsigned accessors are distinct on purpose -- reading a -1 with
// os_read_uint32 yields 4294967295, so the signed variants exist for values
// that can go negative.
// ============================================================================

void registerFFIMemory(RuntimeContext& interp) {
    interp.defineGlobal("os_alloc", Value::makeNativeFunction("os_alloc", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber()) { interp.runtimeError("os_alloc: size must be a number", 0, ""); return Value((long long)0); }
            // Use asInteger() not asNumber(): avoids double-precision loss for
            // sizes > 2^53 (e.g. 2^53+1 silently rounds to 2^53 via asNumber).
            long long reqd = args[0].asInteger();
            if (reqd <= 0) { interp.runtimeError("os_alloc: size must be positive", 0, ""); return Value((long long)0); }
            size_t size = (size_t)reqd;
            void* ptr = calloc(1, size);
            if (ptr) ffiTrackAlloc(reinterpret_cast<uintptr_t>(ptr), size);
            return Value((long long)(reinterpret_cast<uintptr_t>(ptr)));
#else
            return Value();
#endif
        }));


    interp.defineGlobal("os_free", Value::makeNativeFunction("os_free", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber()) return Value();
            uintptr_t addr = (uintptr_t)args[0].asNumber();
            if (!addr) return Value();
            // Only free memory we handed out via os_alloc. Freeing an arbitrary
            // or already-freed address is a heap-corruption / double-free bug.
            if (!ffiUntrackAlloc(addr)) {
                interp.runtimeError("os_free: address was not returned by os_alloc (or already freed)", 0, "");
                return Value();
            }
            free(reinterpret_cast<void*>(addr));
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
            FFI_BOUNDS(interp, args, sizeof(uint64_t), Value(0LL));
            uint64_t val = 0;
            SAFE_MEMORY_OP(interp, val = *(uint64_t*)(base + offset));
            return Value((long long)val);
#else
            return Value(0LL);
#endif
        }));

    // NOTE: os_write_uint64 is registered once below (line ~646) with FFI_BOUNDS.
    // The duplicate entry that was here has been removed.


    interp.defineGlobal("os_read_int64", Value::makeNativeFunction("os_read_int64", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0LL);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            FFI_BOUNDS(interp, args, sizeof(int64_t), Value(0LL));
            int64_t val = 0;
            SAFE_MEMORY_OP(interp, val = *(int64_t*)(base + offset));
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
            FFI_BOUNDS(interp, args, sizeof(int64_t), Value());
            SAFE_MEMORY_OP(interp, *(int64_t*)(base + offset) = (int64_t)args[2].asNumber(););
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
            FFI_BOUNDS(interp, args, sizeof(uint32_t), Value(0LL));
            uint32_t val = 0;
            SAFE_MEMORY_OP(interp, val = *(uint32_t*)(base + offset));
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
            FFI_BOUNDS(interp, args, sizeof(int32_t), Value(0LL));
            int32_t val = 0;
            SAFE_MEMORY_OP(interp, val = *(int32_t*)(base + offset));
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
            FFI_BOUNDS(interp, args, sizeof(uint32_t), Value());
            SAFE_MEMORY_OP(interp, *(uint32_t*)(base + offset) = (uint32_t)args[2].asNumber(););
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
            FFI_BOUNDS(interp, args, sizeof(int32_t), Value());
            SAFE_MEMORY_OP(interp, *(int32_t*)(base + offset) = (int32_t)args[2].asNumber(););
            return Value();
#else
            return Value();
#endif
        }));




    interp.defineGlobal("os_read_uint16", Value::makeNativeFunction("os_read_uint16", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0LL);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            FFI_BOUNDS(interp, args, sizeof(uint16_t), Value(0LL));
            uint16_t val = 0;
            SAFE_MEMORY_OP(interp, val = *(uint16_t*)(base + offset));
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
            FFI_BOUNDS(interp, args, sizeof(int16_t), Value(0LL));
            int16_t val = 0;
            SAFE_MEMORY_OP(interp, val = *(int16_t*)(base + offset));
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
            FFI_BOUNDS(interp, args, sizeof(uint16_t), Value());
            SAFE_MEMORY_OP(interp, *(uint16_t*)(base + offset) = (uint16_t)args[2].asNumber(););
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
            FFI_BOUNDS(interp, args, sizeof(int16_t), Value());
            SAFE_MEMORY_OP(interp, *(int16_t*)(base + offset) = (int16_t)args[2].asNumber(););
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
            FFI_BOUNDS(interp, args, sizeof(uint64_t), Value());
            SAFE_MEMORY_OP(interp, *(uint64_t*)(base + offset) = (uint64_t)args[2].asNumber(););
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
            FFI_BOUNDS(interp, args, sizeof(float), Value(0.0));
            float val = 0;
            SAFE_MEMORY_OP(interp, val = *(float*)(base + offset));
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
            FFI_BOUNDS(interp, args, sizeof(float), Value());
            SAFE_MEMORY_OP(interp, *(float*)(base + offset) = (float)args[2].asFloat(););
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
            FFI_BOUNDS(interp, args, sizeof(double), Value(0.0));
            double val = 0;
            SAFE_MEMORY_OP(interp, val = *(double*)(base + offset));
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
            FFI_BOUNDS(interp, args, sizeof(double), Value());
            SAFE_MEMORY_OP(interp, *(double*)(base + offset) = args[2].asFloat(););
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
            FFI_BOUNDS(interp, args, sizeof(double), Value(0.0));
            double val = 0;
            SAFE_MEMORY_OP(interp, val = *(double*)(base + offset));
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
            FFI_BOUNDS(interp, args, sizeof(double), Value());
            SAFE_MEMORY_OP(interp, *(double*)(base + offset) = args[2].asFloat(););
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
            FFI_BOUNDS(interp, args, sizeof(double), Value(0.0));
            double val = 0;
            SAFE_MEMORY_OP(interp, val = *(double*)(base + offset));
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
            FFI_BOUNDS(interp, args, sizeof(double), Value());
            SAFE_MEMORY_OP(interp, *(double*)(base + offset) = args[2].asFloat(););
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
            FFI_BOUNDS(interp, args, sizeof(uint8_t), Value(0LL));
            uint8_t val = 0;
            SAFE_MEMORY_OP(interp, val = *(base + offset));
            return Value((long long)val);
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
            FFI_BOUNDS(interp, args, sizeof(uint8_t), Value());
            SAFE_MEMORY_OP(interp, *(base + offset) = (uint8_t)args[2].asNumber());
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
            if (!str) return Value("");
            // The only read that cannot be bounds-checked: the length is not
            // known until the terminating NUL is found, so there is no span to
            // validate up front. A string without a NUL inside the allocation
            // will read past it, caught only by the crash guard. Use
            // os_read_string_ptr_n when the data may not be terminated.
            std::string res;
            SAFE_MEMORY_OP(interp, res = std::string(str));
            return Value(res);
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
            // Include the trailing NUL in the bounds check so the whole memcpy is
            // validated against the destination allocation.
            FFI_BOUNDS(interp, args, text.length() + 1, Value());
            SAFE_MEMORY_OP(interp, memcpy(base + offset, text.c_str(), text.length() + 1));
            return Value();
#else
            return Value();
#endif
        }));


    interp.defineGlobal("os_read_string_ptr_n", Value::makeNativeFunction("os_read_string_ptr_n", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value("");
            const char* s = reinterpret_cast<const char*>((uintptr_t)args[0].asNumber());
            size_t maxLen = (size_t)args[1].asInteger();
            if (!s) return Value("");
            // maxLen is a CAP, not a promise that many bytes are readable:
            // ffi.string_at passes 65536 to mean "stop at the NUL, and never
            // scan past 64K". Clamp it to what the tracked block actually holds
            // instead of rejecting -- rejecting broke every caller that passes a
            // generous cap for a short string. Untracked pointers are unchanged.
            maxLen = ffiClampToBlock((uintptr_t)args[0].asNumber(), maxLen);
            std::string res;
            SAFE_MEMORY_OP(interp, res = std::string(s, strnlen(s, maxLen)));
            return Value(res);
#else
            return Value("");
#endif
        }));

    // Lets EZ-level Pointer.free() distinguish callback trampolines (ffi_closure)
    // from plain os_alloc blocks so it can dispatch correctly.
}
