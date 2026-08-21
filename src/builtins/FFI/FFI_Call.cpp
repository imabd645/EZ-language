#include "FFI_Internal.h"

// ============================================================================
// Loading libraries and calling into them.
//
// os_call passes every argument in an integer/pointer register and so cannot
// pass a real float or double; os_call_sig takes per-argument types and can.
// os_call_sig_arr is the same with the arguments as an array, lifting the
// fixed argument-count ceiling.
//
// Cross-platform: the helper functions (type mapping, marshalling, return
// reading) use only libffi types and are fully portable. Only library loading
// (LoadLibrary vs dlopen) and the crash guard inside ffi_call_helper have
// platform-specific branches.
// ============================================================================

// Map an EZ FFI type name to the libffi type used for a RETURN value.
//
// Getting this right is what lets libffi widen a sub-register return correctly.
// A C function that returns a 32-bit int leaves the upper 32 bits of the return
// register undefined, so reading the result as 64 bits gives garbage -- e.g. a
// returned -7 read back as 4294967289. Told the true type (ffi_type_sint32),
// libffi sign-extends it to the register width and the read is correct. Unsigned
// types are zero-extended. Anything unrecognised (including "int" and "i64")
// defaults to 64-bit signed, which is what EZ integers are.
static ffi_type* ffiReturnType(const std::string& t) {
    if (t == "void")                 return &ffi_type_void;
    // "float" means a 64-bit double here: EZ's number type IS a double, and the
    // FFI convention (and every existing caller, e.g. lib/db.ez reading
    // sqlite3_column_double as "float") has always treated it that way. Only the
    // explicit "f32" is a 32-bit C float.
    if (t == "f32")                  return &ffi_type_float;
    if (t == "float" || t == "double" || t == "f64") return &ffi_type_double;
    if (t == "i8")                   return &ffi_type_sint8;
    if (t == "u8")                   return &ffi_type_uint8;
    if (t == "i16")                  return &ffi_type_sint16;
    if (t == "u16")                  return &ffi_type_uint16;
    if (t == "i32")                  return &ffi_type_sint32;
    if (t == "u32")                  return &ffi_type_uint32;
    if (t == "u64")                  return &ffi_type_uint64;
    if (t == "ptr")                  return &ffi_type_pointer;
    return &ffi_type_sint64; // "int", "i64", and the default
}

// The register-sized buffer a libffi call writes its return value into. libffi
// widens every integer return to at least ffi_arg (8 bytes on x64) with the
// correct sign/zero extension for the ffi_type, so the integer members read back
// directly. float/double are stored at their own width.
union FfiRet { intptr_t i; uint64_t u; double d; float f; };

// Turn a completed FfiRet into an EZ Value according to the declared return type.
static Value ffiReadReturn(const std::string& retType, const FfiRet& r) {
    if (retType == "void")   return Value();
    // "f32" is a real 32-bit float; "float" (like "double"/"f64") is 64-bit.
    if (retType == "f32")    return Value((double)r.f);
    if (retType == "float" || retType == "double" || retType == "f64") return Value(r.d);
    if (retType == "string") {
        const char* s = reinterpret_cast<const char*>(r.i);
        return s ? Value(std::string(s)) : Value("");
    }
    // Unsigned reads: libffi already zero-extended, so the low bits are the
    // value; mask to be explicit. u64 above 2^63 cannot be represented in EZ's
    // signed 64-bit integer and comes back negative -- a documented limit.
    if (retType == "u8")  return Value((long long)(uint8_t)r.u);
    if (retType == "u16") return Value((long long)(uint16_t)r.u);
    if (retType == "u32") return Value((long long)(uint32_t)r.u);
    if (retType == "u64") return Value((long long)r.u);
    // Signed and pointer: libffi sign-extended smaller signed types already.
    return Value((long long)r.i);
}

// Marshal one EZ Value into an integer/pointer-register argument slot. Integers
// go through asInteger() -- NOT asNumber(), which is a double and silently loses
// precision above 2^53 (an int64 argument would arrive mangled). Floats still
// pass through here for os_call, which lacks per-argument typing; use os_call_sig
// for a genuine floating-point parameter.
static intptr_t ffiMarshalIntArg(const Value& v, std::string& tempStr) {
    if (v.isInteger())      return (intptr_t)v.asInteger();
    if (v.isFloat())        return (intptr_t)v.asFloat();
    if (v.isString())     { tempStr = v.asString(); return reinterpret_cast<intptr_t>(tempStr.c_str()); }
    if (v.isBuffer())       return reinterpret_cast<intptr_t>(v.asBuffer().data());
    if (v.isBool())         return v.asBool() ? 1 : 0;
    return 0;
}

// ── Cross-platform crash-guarded FFI call ───────────────────────────────────
// Wraps ffi_call with a platform-specific crash guard so that an access
// violation or segfault inside the native function surfaces as a catchable
// EZ runtime error rather than killing the interpreter process.
static bool ffi_call_helper(void* funcPtr, size_t argc, ffi_type** argTypes, void** argValues, ffi_type* retType, void* retBuffer, RuntimeContext& interp) {
    ffi_cif cif;
    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, (unsigned int)argc, retType, argTypes) == FFI_OK) {
        bool crashed = false;

#ifdef _WIN32
#ifdef _MSC_VER
        __try {
            ffi_call(&cif, reinterpret_cast<void(*)()>(funcPtr), retBuffer, argValues);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            crashed = true;
        }
#else
        PVOID vehHandler = AddVectoredExceptionHandler(1, FfiVectoredHandler);
        if (setjmp(os_call_jmp_env) == 0) {
            ffi_call(&cif, reinterpret_cast<void(*)()>(funcPtr), retBuffer, argValues);
        } else {
            crashed = true;
        }
        RemoveVectoredExceptionHandler(vehHandler);
#endif
#else
        // POSIX: use sigsetjmp/siglongjmp via the same guard as SAFE_MEMORY_OP.
        posix_ffi_guard_active = 1;
        if (sigsetjmp(posix_ffi_jmp_env, 1) == 0) {
            ffi_call(&cif, reinterpret_cast<void(*)()>(funcPtr), retBuffer, argValues);
        } else {
            crashed = true;
        }
        posix_ffi_guard_active = 0;
#endif

        if (crashed) {
            interp.runtimeError("os_call: Access violation or fatal memory fault inside native library.", 0, "");
            return false;
        }
        return true;
    }
    interp.runtimeError("os_call: Failed to prepare FFI CIF", 0, "");
    return false;
}

void registerFFICall(RuntimeContext& interp) {
    // ── os_load_lib ─────────────────────────────────────────────────────────
    // Load a shared library by name. Returns the handle as an integer.
    // Windows: LoadLibraryA  |  POSIX: dlopen
    interp.defineGlobal("os_load_lib", Value::makeNativeFunction("os_load_lib", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) return Value();
#ifdef _WIN32
            HMODULE handle = LoadLibraryA(args[0].asString().c_str());
            return Value((long long)(reinterpret_cast<uintptr_t>(handle)));
#else
            void* handle = dlopen(args[0].asString().c_str(), RTLD_LAZY);
            return Value((long long)(reinterpret_cast<uintptr_t>(handle)));
#endif
        }));


        interp.defineGlobal("os_free_lib", Value::makeNativeFunction("os_free_lib", 1,
    [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
        if (!args[0].isNumber()) return Value();
#ifdef _WIN32
        HMODULE handle = reinterpret_cast<HMODULE>((uintptr_t)args[0].asNumber());
        FreeLibrary(handle);
#else
        void* handle = reinterpret_cast<void*>((uintptr_t)args[0].asNumber());
        dlclose(handle);
#endif
        return Value();
    }));


    // ── os_get_func ─────────────────────────────────────────────────────────
    // Look up a function symbol in a loaded library. Returns the pointer as an integer.
    // Windows: GetProcAddress  |  POSIX: dlsym
    interp.defineGlobal("os_get_func", Value::makeNativeFunction("os_get_func", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber() || !args[1].isString()) return Value();
#ifdef _WIN32
            HMODULE handle = reinterpret_cast<HMODULE>((uintptr_t)args[0].asNumber());
            FARPROC proc = GetProcAddress(handle, args[1].asString().c_str());
            return Value((long long)(reinterpret_cast<uintptr_t>(proc)));
#else
            void* handle = reinterpret_cast<void*>((uintptr_t)args[0].asNumber());
            void* proc = dlsym(handle, args[1].asString().c_str());
            return Value((long long)(reinterpret_cast<uintptr_t>(proc)));
#endif
        }));


    // ── os_call ─────────────────────────────────────────────────────────────
    // Call a native function pointer. All arguments are passed as integer/pointer
    // register slots (no floating-point); use os_call_sig for float parameters.
    interp.defineGlobal("os_call", Value::makeNativeFunction("os_call", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.size() < 2 || !args[0].isNumber() || !args[1].isString()) return Value();
            void* funcPtr = reinterpret_cast<void*>((uintptr_t)args[0].asNumber());
            if (!funcPtr) {
                interp.runtimeError("os_call: Null function pointer or function not found.", 0, "");
                return Value();
            }
            
            size_t argc = args.size() - 2;
            std::string retType = args[1].asString();
            
            // Every argument occupies one integer/pointer register slot. os_call
            // has no per-argument typing, so a real floating-point parameter must
            // go through os_call_sig; here integers are marshalled EXACTLY (via
            // asInteger(), not a lossy double).
            std::vector<ffi_type*> argTypes(argc, &ffi_type_pointer);
            std::vector<void*> argValues(argc);
            std::vector<intptr_t> cArgs(argc, 0);
            std::vector<std::string> tempStrings(argc);

            for (size_t i = 0; i < argc; i++) {
                cArgs[i] = ffiMarshalIntArg(args[i + 2], tempStrings[i]);
                argValues[i] = &cArgs[i];
            }

            ffi_type* rType = ffiReturnType(retType);
            FfiRet retBuffer; retBuffer.i = 0;

            if (!ffi_call_helper(funcPtr, argc, argTypes.data(), argValues.data(), rType, &retBuffer, interp)) {
                return Value();
            }
            return ffiReadReturn(retType, retBuffer);
        }));


    // ── os_call_sig ─────────────────────────────────────────────────────────
    // Call a native function with per-argument type specification, allowing
    // proper floating-point register passing.
    interp.defineGlobal("os_call_sig", Value::makeNativeFunction("os_call_sig", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.size() < 3 || !args[0].isNumber() || !args[1].isString() || !args[2].isArray()) return Value();
            void* funcPtr = reinterpret_cast<void*>((uintptr_t)args[0].asNumber());
            if (!funcPtr) {
                interp.runtimeError("os_call_sig: Null function pointer.", 0, "");
                return Value();
            }
            
            std::string retType = args[1].asString();
            auto sigArray = args[2].asArray().getElementsCopy();
            size_t argc = sigArray.size();
            
            std::vector<ffi_type*> argTypes(argc);
            std::vector<void*> argValues(argc);
            std::vector<intptr_t> iArgs(argc, 0);
            std::vector<double> fArgs(argc, 0.0);
            std::vector<std::string> tempStrings(argc);
            
            for (size_t i = 0; i < argc; i++) {
                size_t valIdx = i + 3;
                if (valIdx >= args.size()) break;

                std::string type = sigArray[i].isString() ? sigArray[i].asString() : "ptr";

                if (type == "float" || type == "double" || type == "f32" || type == "f64") {
                    // A double parameter must land in an XMM register: give it the
                    // real floating type, not a reinterpreted integer.
                    argTypes[i] = &ffi_type_double;
                    if (args[valIdx].isInteger())    fArgs[i] = (double)args[valIdx].asInteger();
                    else if (args[valIdx].isNumber()) fArgs[i] = args[valIdx].asFloat();
                    argValues[i] = &fArgs[i];
                } else {
                    // Integers marshalled exactly (asInteger(), not a lossy double).
                    argTypes[i] = &ffi_type_pointer;
                    iArgs[i] = ffiMarshalIntArg(args[valIdx], tempStrings[i]);
                    argValues[i] = &iArgs[i];
                }
            }

            ffi_type* rType = ffiReturnType(retType);
            FfiRet retBuffer; retBuffer.i = 0;

            if (!ffi_call_helper(funcPtr, argc, argTypes.data(), argValues.data(), rType, &retBuffer, interp)) {
                return Value();
            }
            return ffiReadReturn(retType, retBuffer);
        }));


    // ── os_call_sig_arr ─────────────────────────────────────────────────────
    // Same as os_call_sig but with arguments passed as an array, lifting the
    // fixed argument-count ceiling.
    interp.defineGlobal("os_call_sig_arr", Value::makeNativeFunction("os_call_sig_arr", 4,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber() || !args[1].isString() || !args[2].isArray() || !args[3].isArray()) {
                interp.runtimeError("os_call_sig_arr expects (ptr, retType, sigArray, argsArray)", 0, "");
                return Value();
            }
            void* funcPtr = reinterpret_cast<void*>((uintptr_t)args[0].asNumber());
            if (!funcPtr) { interp.runtimeError("os_call_sig_arr: null function pointer", 0, ""); return Value(); }

            std::string retType  = args[1].asString();
            auto sigArray        = args[2].asArray().getElementsCopy();
            auto userArgs        = args[3].asArray().getElementsCopy();
            size_t argc          = sigArray.size();

            std::vector<ffi_type*>   argTypes(argc);
            std::vector<void*>       argValues(argc);
            std::vector<intptr_t>    iArgs(argc, 0);
            std::vector<double>      fArgs(argc, 0.0);
            std::vector<std::string> tempStrings(argc);

            for (size_t i = 0; i < argc; i++) {
                std::string type = sigArray[i].isString() ? sigArray[i].asString() : "ptr";
                const Value& val = (i < userArgs.size()) ? userArgs[i] : Value();

                if (type == "float" || type == "double" || type == "f32" || type == "f64") {
                    argTypes[i]  = &ffi_type_double;
                    if (val.isInteger())     fArgs[i] = (double)val.asInteger();
                    else if (val.isNumber()) fArgs[i] = val.asFloat();
                    argValues[i] = &fArgs[i];
                } else {
                    argTypes[i]  = &ffi_type_pointer;
                    iArgs[i]     = ffiMarshalIntArg(val, tempStrings[i]);
                    argValues[i] = &iArgs[i];
                }
            }

            ffi_type* rType = ffiReturnType(retType);
            FfiRet retBuffer; retBuffer.i = 0;
            if (!ffi_call_helper(funcPtr, argc, argTypes.data(), argValues.data(), rType, &retBuffer, interp))
                return Value();
            return ffiReadReturn(retType, retBuffer);
        }));
}
