#ifndef VALUE_H
#define VALUE_H

#include <memory>
#include <vector>
#include <string>
#include <variant>
#include <functional>
#include <unordered_map>
#include <future>
#include "AST.h"
#include "GCObject.h"

// Forward declarations
class Environment;
class Interpreter;
struct Value;

using ValuePtr = std::shared_ptr<Value>;

// Function types
struct EZFunction;
struct NativeFunction;
struct EZClass;
struct EZInstance;
struct EZDictionary;
struct EZSuper;

using NativeFn = std::function<Value(Interpreter&, const std::vector<Value>&)>;

enum class ValueType {
    NIL,
    BOOL,
    NUMBER,
    STRING,
    ARRAY,
    FUNCTION,
    NATIVE_FUNCTION,
    CLASS,
    INSTANCE,
    DICTIONARY,
    FUTURE,
    SUPER,
    INTEGER,
    BUFFER
};

struct EZFunction : public GCObject {
    std::string name;
    std::vector<std::string> params;
    std::vector<ExprPtr> defaultValues;
    std::vector<StmtPtr> body;
    std::shared_ptr<Environment> closure;
    std::shared_ptr<Environment> staticEnv;
    bool isVariadic;
    
    EZFunction(const std::string& name, 
               const std::vector<std::string>& params,
               const std::vector<ExprPtr>& defaultValues,
               const std::vector<StmtPtr>& body,
               std::shared_ptr<Environment> closure,
               bool variadic = false);

    void gc_mark() override;
    void gc_clear() override { closure = nullptr; staticEnv = nullptr; }
};

// Native (built-in) function
struct NativeFunction {
    std::string name;
    int arity; // -1 for variadic
    NativeFn function;
    
    NativeFunction(const std::string& name, int arity, NativeFn fn)
        : name(name), arity(arity), function(fn) {}
};

struct EZArray : public GCObject {
    std::vector<Value> elements;
    EZArray(const std::vector<Value>& e = {}) : elements(e) {}
    void gc_mark() override;
    void gc_clear() override { elements.clear(); }
    
    size_t size() const { return elements.size(); }
    bool empty() const { return elements.empty(); }
    void push_back(const Value& v) { elements.push_back(v); }
    void pop_back() { elements.pop_back(); }
    Value& back() { return elements.back(); }
    Value& operator[](size_t i) { return elements[i]; }
    const Value& operator[](size_t i) const { return elements[i]; }
    auto begin() { return elements.begin(); }
    auto end() { return elements.end(); }
    void insert(std::vector<Value>::iterator it, const Value& v) { elements.insert(it, v); }
    void erase(std::vector<Value>::iterator it) { elements.erase(it); }
};

struct EZDictionary : public GCObject {
    std::unordered_map<std::string, Value> map;
    void gc_mark() override;
    void gc_clear() override { map.clear(); }
};

struct EZBuffer : public GCObject {
    std::vector<uint8_t> data;
    EZBuffer(size_t size = 0) : data(size, 0) {}
    EZBuffer(const std::vector<uint8_t>& d) : data(d) {}
    void gc_mark() override { gc_marked = true; } // Buffers don't contain other Values
    void gc_clear() override { data.clear(); }
    
    size_t size() const { return data.size(); }
    uint8_t& operator[](size_t i) { return data[i]; }
    const uint8_t& operator[](size_t i) const { return data[i]; }
};

// The main Value struct - dynamically typed
struct Value {
    using ArrayType = std::vector<Value>;
    using StringPtr = std::shared_ptr<std::string>;
    using ArrayPtr = std::shared_ptr<EZArray>;
    using FunctionPtr = std::shared_ptr<EZFunction>;
    using NativeFnPtr = std::shared_ptr<NativeFunction>;
    using ClassPtr = std::shared_ptr<EZClass>;
    using InstancePtr = std::shared_ptr<EZInstance>;
    using DictionaryPtr = std::shared_ptr<EZDictionary>;
    using FuturePtr = std::shared_ptr<std::shared_future<Value>>;
    using SuperPtr = std::shared_ptr<EZSuper>;
    using BufferPtr = std::shared_ptr<EZBuffer>;
    
    std::variant<
        std::nullptr_t,     // NIL
        bool,               // BOOL
        double,             // NUMBER
        StringPtr,          // STRING
        ArrayPtr,           // ARRAY
        FunctionPtr,        // FUNCTION
        NativeFnPtr,        // NATIVE_FUNCTION
        ClassPtr,           // CLASS
        InstancePtr,        // INSTANCE
        DictionaryPtr,      // DICTIONARY
        FuturePtr,          // FUTURE
        SuperPtr,           // SUPER
        long long,          // INTEGER
        BufferPtr           // BUFFER
    > data;
    
    // Constructors
    Value() : data(nullptr) {}
    Value(std::nullptr_t) : data(nullptr) {}
    Value(bool val) : data(val) {}
    Value(double val) : data(val) {}
    Value(int val) : data(static_cast<long long>(val)) {}
    Value(unsigned int val) : data(static_cast<long long>(val)) {}
    Value(long val) : data(static_cast<long long>(val)) {}
    Value(unsigned long val) : data(static_cast<long long>(val)) {}
    Value(long long val) : data(val) {}
    Value(unsigned long long val) : data(static_cast<long long>(val)) {}
    Value(const std::string& val) : data(std::make_shared<std::string>(val)) {}
    Value(const char* val) : data(std::make_shared<std::string>(val)) {}
    Value(StringPtr val) : data(val) {}
    Value(ArrayPtr val) : data(val) {}
    Value(FunctionPtr val) : data(val) {}
    Value(NativeFnPtr val) : data(val) {}
    Value(ClassPtr val) : data(val) {}
    Value(InstancePtr val) : data(val) {}
    Value(DictionaryPtr val) : data(val) {}
    Value(FuturePtr val) : data(val) {}
    Value(SuperPtr val) : data(val) {}
    // Deleted duplicate long long constructor
    
    Value(BufferPtr val) : data(val) {}
    
    // Type checking
    ValueType type() const {
        if (std::holds_alternative<std::nullptr_t>(data)) return ValueType::NIL;
        if (std::holds_alternative<bool>(data)) return ValueType::BOOL;
        if (std::holds_alternative<double>(data)) return ValueType::NUMBER;
        if (std::holds_alternative<StringPtr>(data)) return ValueType::STRING;
        if (std::holds_alternative<ArrayPtr>(data)) return ValueType::ARRAY;
        if (std::holds_alternative<FunctionPtr>(data)) return ValueType::FUNCTION;
        if (std::holds_alternative<NativeFnPtr>(data)) return ValueType::NATIVE_FUNCTION;
        if (std::holds_alternative<ClassPtr>(data)) return ValueType::CLASS;
        if (std::holds_alternative<InstancePtr>(data)) return ValueType::INSTANCE;
        if (std::holds_alternative<DictionaryPtr>(data)) return ValueType::DICTIONARY;
        if (std::holds_alternative<FuturePtr>(data)) return ValueType::FUTURE;
        if (std::holds_alternative<SuperPtr>(data)) return ValueType::SUPER;
        if (std::holds_alternative<long long>(data)) return ValueType::INTEGER;
        if (std::holds_alternative<BufferPtr>(data)) return ValueType::BUFFER;
        return ValueType::NIL;
    }
    
    bool isNil() const { return std::holds_alternative<std::nullptr_t>(data); }
    bool isBool() const { return std::holds_alternative<bool>(data); }
    bool isInteger() const { return std::holds_alternative<long long>(data); }
    bool isFloat() const { return std::holds_alternative<double>(data); }
    bool isNumber() const { return isInteger() || isFloat(); }
    bool isString() const { return std::holds_alternative<StringPtr>(data); }
    bool isArray() const { return std::holds_alternative<ArrayPtr>(data); }
    bool isFunction() const { return std::holds_alternative<FunctionPtr>(data); }
    bool isNativeFunction() const { return std::holds_alternative<NativeFnPtr>(data); }
    bool isClass() const { return std::holds_alternative<ClassPtr>(data); }
    bool isInstance() const { return std::holds_alternative<InstancePtr>(data); }
    bool isDictionary() const { return std::holds_alternative<DictionaryPtr>(data); }
    bool isFuture() const { return std::holds_alternative<FuturePtr>(data); }
    bool isSuper() const { return std::holds_alternative<SuperPtr>(data); }
    bool isBuffer() const { return std::holds_alternative<BufferPtr>(data); }
    bool isCallable() const { return isFunction() || isNativeFunction() || isClass(); }
    
    // Value extraction
    bool asBool() const { return std::get<bool>(data); }
    double asFloat() const { 
        if (isInteger()) return static_cast<double>(std::get<long long>(data));
        return std::get<double>(data); 
    }
    long long asInteger() const { 
        if (isFloat()) return static_cast<long long>(std::get<double>(data));
        return std::get<long long>(data); 
    }
    double asNumber() const { return asFloat(); }
    StringPtr asStringPtr() const { return std::get<StringPtr>(data); }
    const std::string& asString() const { return *std::get<StringPtr>(data); }
    ArrayPtr asArrayPtr() const { return std::get<ArrayPtr>(data); }
    std::vector<Value>& asArray() { return std::get<ArrayPtr>(data)->elements; }
    const std::vector<Value>& asArray() const { return std::get<ArrayPtr>(data)->elements; }
    FunctionPtr asFunction() const { return std::get<FunctionPtr>(data); }
    NativeFnPtr asNativeFunction() const { return std::get<NativeFnPtr>(data); }
    ClassPtr asClass() const { return std::get<ClassPtr>(data); }
    InstancePtr asInstance() const { return std::get<InstancePtr>(data); }
    DictionaryPtr asDictionaryPtr() const { return std::get<DictionaryPtr>(data); }
    FuturePtr asFuture() const { return std::get<FuturePtr>(data); }
    SuperPtr asSuper() const { return std::get<SuperPtr>(data); }
    BufferPtr asBufferPtr() const { return std::get<BufferPtr>(data); }
    std::vector<uint8_t>& asBuffer() const { return std::get<BufferPtr>(data)->data; }
    EZDictionary& asDictionary();
    const EZDictionary& asDictionary() const;
    
    // Truthiness - everything is true except nil and false
    bool isTruthy() const {
        if (isNil()) return false;
        if (isBool()) return asBool();
        return true;
    }
    
    // Equality
    bool equals(const Value& other) const {
        if (isNumber() && other.isNumber()) {
            if (isInteger() && other.isInteger()) return asInteger() == other.asInteger();
            return asNumber() == other.asNumber();
        }
        if (type() != other.type()) return false;
        
        switch (type()) {
            case ValueType::NIL: return true;
            case ValueType::BOOL: return asBool() == other.asBool();
            case ValueType::STRING: return asString() == other.asString();
            case ValueType::ARRAY: {
                const auto& a = asArray();
                const auto& b = other.asArray();
                if (a.size() != b.size()) return false;
                for (size_t i = 0; i < a.size(); i++) {
                    if (!a[i].equals(b[i])) return false;
                }
                return true;
            }
            default: return false; 
        }
    }
    
    // String conversion
    std::string toString() const;
    
    // Type name
    std::string typeName() const;
    
    // Create array
    static Value makeArray(const std::vector<Value>& elements = {}) {
        return Value(std::make_shared<EZArray>(elements));
    }
    
    // Create function
    static Value makeFunction(const std::string& name,
                              const std::vector<std::string>& params,
                              const std::vector<ExprPtr>& defaultValues,
                              const std::vector<StmtPtr>& body,
                              std::shared_ptr<Environment> closure,
                              bool variadic = false) {
        return Value(std::make_shared<EZFunction>(name, params, defaultValues, body, closure, variadic));
    }
    
    // Create native function
    static Value makeNativeFunction(const std::string& name, int arity, NativeFn fn) {
        return Value(std::make_shared<NativeFunction>(name, arity, fn));
    }
    
    // Create dictionary
    static Value makeDictionary();
    
    // Create future
    static Value makeFuture(std::shared_future<Value> fut) {
        return Value(std::make_shared<std::shared_future<Value>>(fut));
    }
    
    // Create super proxy
    static Value makeSuper(InstancePtr instance, ClassPtr parentKlass);
};

// EZ Class definition (model)
struct EZClass : public GCObject {
    std::string name;
    std::shared_ptr<EZClass> parent;
    std::vector<std::string> initParams;
    std::vector<StmtPtr> initBody;
    std::unordered_map<std::string, Value> methods;
    std::unordered_map<std::string, Value> staticMembers;
    std::unordered_map<std::string, bool> visibility;  // true = public (shown)
    
    EZClass(const std::string& name) : name(name), parent(nullptr) {}
    void gc_mark() override;
    void gc_clear() override { parent = nullptr; methods.clear(); staticMembers.clear(); }
};

// EZ Instance (object created from model)
struct EZInstance : public GCObject {
    std::shared_ptr<EZClass> klass;
    std::unordered_map<std::string, Value> properties;
    
    EZInstance(std::shared_ptr<EZClass> klass) : klass(klass) {}
    
    bool hasProperty(const std::string& name) const {
        return properties.find(name) != properties.end();
    }
    
    Value getProperty(const std::string& name) const {
        auto it = properties.find(name);
        if (it != properties.end()) return it->second;
        return Value();  // nil
    }
    
    void setProperty(const std::string& name, const Value& value) {
        properties[name] = value;
    }
    void gc_mark() override;
    void gc_clear() override { properties.clear(); klass = nullptr; }
};

struct EZSuper {
    std::shared_ptr<EZInstance> instance;
    std::shared_ptr<EZClass> parentKlass;
    
    EZSuper(std::shared_ptr<EZInstance> instance, std::shared_ptr<EZClass> parentKlass) 
        : instance(instance), parentKlass(parentKlass) {}
};

// EZDictionary is already handled above as a GCObject proxy. Removing the old struct.
#if 0
struct EZDictionary {
    std::unordered_map<std::string, Value> map;
};
#endif

inline EZDictionary& Value::asDictionary() { return *std::get<DictionaryPtr>(data); }
inline const EZDictionary& Value::asDictionary() const { return *std::get<DictionaryPtr>(data); }
inline Value Value::makeDictionary() { return Value(std::make_shared<EZDictionary>()); } 
inline Value Value::makeSuper(InstancePtr instance, ClassPtr parentKlass) { return Value(std::make_shared<EZSuper>(instance, parentKlass)); }

inline std::string Value::toString() const {
    switch (type()) {
        case ValueType::NIL: return "nil";
        case ValueType::BOOL: return asBool() ? "true" : "false";
        case ValueType::INTEGER: return std::to_string(asInteger());
        case ValueType::NUMBER: {
            double num = asNumber();
            if (num == static_cast<int>(num)) {
                return std::to_string(static_cast<int>(num));
            }
            return std::to_string(num);
        }
        case ValueType::STRING: return asString();
        case ValueType::ARRAY: {
            std::string result = "[";
            const auto& arr = asArray();
            for (size_t i = 0; i < arr.size(); i++) {
                if (i > 0) result += ", ";
                if (arr[i].isString()) {
                    result += "\"" + arr[i].toString() + "\"";
                } else {
                    result += arr[i].toString();
                }
            }
            result += "]";
            return result;
        }
        case ValueType::FUNCTION:
            return "<function " + asFunction()->name + ">";
        case ValueType::NATIVE_FUNCTION:
            return "<native fn " + asNativeFunction()->name + ">";
        case ValueType::CLASS:
            return "<model " + asClass()->name + ">";
        case ValueType::INSTANCE:
            return "<" + asInstance()->klass->name + " instance>";
        case ValueType::DICTIONARY:
            return "<dictionary>";
        case ValueType::FUTURE:
            return "<future>";
        default:
            return "<unknown>";
    }
}

inline std::string Value::typeName() const {
    switch (type()) {
        case ValueType::NIL: return "nil";
        case ValueType::BOOL: return "bool";
        case ValueType::INTEGER: return "number";
        case ValueType::NUMBER: return "number";
        case ValueType::STRING: return "string";
        case ValueType::ARRAY: return "array";
        case ValueType::FUNCTION: return "function";
        case ValueType::NATIVE_FUNCTION: return "function";
        case ValueType::CLASS: return "model";
        case ValueType::INSTANCE: return "instance";
        case ValueType::DICTIONARY: return "dictionary";
        case ValueType::FUTURE: return "future";
        default: return "unknown";
    }
}

#endif // VALUE_H
