#include "../Builtins.h"
#include "../Interpreter.h"
#include "../GC.h"
#include <string>
#include <vector>
#include <algorithm>

void registerBufferBuiltins(Interpreter& interp) {
    // buffer(size) or buffer(string)
    interp.defineGlobal("buffer", Value::makeNativeFunction("buffer", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (args[0].isNumber()) {
                int size = static_cast<int>(args[0].asInteger());
                if (size < 0) { interp.runtimeError("buffer() size cannot be negative", 0, ""); return Value(); }
                return Value(makeGCBuffer(size));
            } else if (args[0].isString()) {
                const std::string& str = args[0].asString();
                std::vector<uint8_t> data(str.begin(), str.end());
                return Value(makeGCBuffer(data));
            } else {
                interp.runtimeError("buffer() expects size (number) or string", 0, "");
                return Value();
            }
        }));

    // buf_size(buffer)
    interp.defineGlobal("buf_size", Value::makeNativeFunction("buf_size", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isBuffer()) { interp.runtimeError("buf_size() expects buffer", 0, ""); return Value(); }
            return Value(static_cast<long long>(args[0].asBufferPtr()->size()));
        }));

    // buf_fill(buffer, byte_value)
    interp.defineGlobal("buf_fill", Value::makeNativeFunction("buf_fill", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isBuffer()) { interp.runtimeError("buf_fill() expects buffer", 0, ""); return Value(); }
            if (!args[1].isNumber()) { interp.runtimeError("buf_fill() expects numeric byte value", 0, ""); return Value(); }
            uint8_t val = static_cast<uint8_t>(args[1].asInteger());
            std::vector<uint8_t>& data = args[0].asBuffer();
            std::fill(data.begin(), data.end(), val);
            return args[0];
        }));

    // buf_copy(src, target, [targetStart], [srcStart], [srcEnd])
    interp.defineGlobal("buf_copy", Value::makeNativeFunction("buf_copy", -1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (args.size() < 2) { interp.runtimeError("buf_copy() expects at least 2 arguments (src, target)", 0, ""); return Value(); }
            if (!args[0].isBuffer() || !args[1].isBuffer()) { interp.runtimeError("buf_copy() expects buffers for src and target", 0, ""); return Value(); }
            
            auto& src = args[0].asBuffer();
            auto& dest = args[1].asBuffer();
            
            int targetStart = (args.size() > 2) ? static_cast<int>(args[2].asInteger()) : 0;
            int srcStart = (args.size() > 3) ? static_cast<int>(args[3].asInteger()) : 0;
            int srcEnd = (args.size() > 4) ? static_cast<int>(args[4].asInteger()) : static_cast<int>(src.size());
            
            if (srcStart < 0 || srcEnd > static_cast<int>(src.size()) || srcStart > srcEnd) {
                interp.runtimeError("buf_copy() source range out of bounds", 0, "");
                return Value();
            }
            
            int len = srcEnd - srcStart;
            if (targetStart < 0 || targetStart + len > static_cast<int>(dest.size())) {
                interp.runtimeError("buf_copy() target range out of bounds", 0, "");
                return Value();
            }
            
            if (len > 0) {
                std::copy(src.begin() + srcStart, src.begin() + srcEnd, dest.begin() + targetStart);
            }
            
            return Value(static_cast<long long>(len));
        }));

    // buf_to_str(buffer)
    interp.defineGlobal("buf_to_str", Value::makeNativeFunction("buf_to_str", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isBuffer()) { interp.runtimeError("buf_to_str() expects buffer", 0, ""); return Value(); }
            auto& data = args[0].asBuffer();
            return Value(std::string(data.begin(), data.end()));
        }));
}
