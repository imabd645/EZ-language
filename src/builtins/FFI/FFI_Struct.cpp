#include "FFI_Internal.h"

// ============================================================================
// Struct layout: size/alignment computation, packing EZ values into a buffer
// and unpacking them back out, following C struct padding rules.
// ============================================================================

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



void registerFFIStruct(RuntimeContext& interp) {
    interp.defineGlobal("os_struct_alloc", Value::makeNativeFunction("os_struct_alloc", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                interp.runtimeError("os_struct_alloc expects an array of strings", 0, "");
                return Value();
            }
            auto layoutArray = args[0].asArray().getElementsCopy();
            StructLayout layout = computeStructLayout(layoutArray);
            return Value(std::make_shared<EZBuffer>(layout.totalSize));
        }));


    interp.defineGlobal("os_struct_pack", Value::makeNativeFunction("os_struct_pack", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.size() < 2 || !args[0].isArray() || !args[1].isArray()) {
                interp.runtimeError("os_struct_pack expects (layout_array, values_array, [buffer])", 0, "");
                return Value();
            }
            auto layoutArray = args[0].asArray().getElementsCopy();
            auto valuesArray = args[1].asArray().getElementsCopy();
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
            auto layoutArray = args[0].asArray().getElementsCopy();
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
            SAFE_MEMORY_OP(interp, {
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
            });
            
            return Value::makeArray(results);
        }));

}
