#ifndef VALUE_H
#define VALUE_H

#include <memory>
#include <vector>
#include <string>
#include <variant>
#include <functional>
#include <unordered_map>
#include "AST.h"

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
    DICTIONARY
};

// EZ user-defined function
struct EZFunction {
    std::string name;
    std::vector<std::string> params;
    std::vector<StmtPtr> body;
    std::shared_ptr<Environment> closure;
    
    EZFunction(const std::string& name, 
               const std::vector<std::string>& params,
               const std::vector<StmtPtr>& body,
               std::shared_ptr<Environment> closure)
        : name(name), params(params), body(body), closure(closure) {}
};

// Native (built-in) function
struct NativeFunction {
    std::string name;
    int arity; // -1 for variadic
    NativeFn function;
    
    NativeFunction(const std::string& name, int arity, NativeFn fn)
        : name(name), arity(arity), function(fn) {}
};

// The main Value struct - dynamically typed
struct Value {
    using ArrayType = std::vector<Value>;
    using StringPtr = std::shared_ptr<std::string>;
    using ArrayPtr = std::shared_ptr<ArrayType>;
    using FunctionPtr = std::shared_ptr<EZFunction>;
    using NativeFnPtr = std::shared_ptr<NativeFunction>;
    using ClassPtr = std::shared_ptr<EZClass>;
    using InstancePtr = std::shared_ptr<EZInstance>;
    using DictionaryPtr = std::shared_ptr<EZDictionary>;
    
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
        DictionaryPtr       // DICTIONARY
    > data;
    
    // Constructors
    Value() : data(nullptr) {}
    Value(std::nullptr_t) : data(nullptr) {}
    Value(bool val) : data(val) {}
    Value(double val) : data(val) {}
    Value(int val) : data(static_cast<double>(val)) {}
    Value(const std::string& val) : data(std::make_shared<std::string>(val)) {}
    Value(const char* val) : data(std::make_shared<std::string>(val)) {}
    Value(StringPtr val) : data(val) {}
    Value(ArrayPtr val) : data(val) {}
    Value(FunctionPtr val) : data(val) {}
    Value(NativeFnPtr val) : data(val) {}
    Value(ClassPtr val) : data(val) {}
    Value(InstancePtr val) : data(val) {}
    Value(DictionaryPtr val) : data(val) {}
    
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
        return ValueType::NIL;
    }
    
    bool isNil() const { return std::holds_alternative<std::nullptr_t>(data); }
    bool isBool() const { return std::holds_alternative<bool>(data); }
    bool isNumber() const { return std::holds_alternative<double>(data); }
    bool isString() const { return std::holds_alternative<StringPtr>(data); }
    bool isArray() const { return std::holds_alternative<ArrayPtr>(data); }
    bool isFunction() const { return std::holds_alternative<FunctionPtr>(data); }
    bool isNativeFunction() const { return std::holds_alternative<NativeFnPtr>(data); }
    bool isClass() const { return std::holds_alternative<ClassPtr>(data); }
    bool isInstance() const { return std::holds_alternative<InstancePtr>(data); }
    bool isDictionary() const { return std::holds_alternative<DictionaryPtr>(data); }
    bool isCallable() const { return isFunction() || isNativeFunction() || isClass(); }
    
    // Value extraction
    bool asBool() const { return std::get<bool>(data); }
    double asNumber() const { return std::get<double>(data); }
    StringPtr asStringPtr() const { return std::get<StringPtr>(data); }
    const std::string& asString() const { return *std::get<StringPtr>(data); }
    ArrayPtr asArrayPtr() const { return std::get<ArrayPtr>(data); }
    ArrayType& asArray() { return *std::get<ArrayPtr>(data); }
    const ArrayType& asArray() const { return *std::get<ArrayPtr>(data); }
    FunctionPtr asFunction() const { return std::get<FunctionPtr>(data); }
    NativeFnPtr asNativeFunction() const { return std::get<NativeFnPtr>(data); }
    ClassPtr asClass() const { return std::get<ClassPtr>(data); }
    InstancePtr asInstance() const { return std::get<InstancePtr>(data); }
    DictionaryPtr asDictionaryPtr() const { return std::get<DictionaryPtr>(data); }
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
        if (type() != other.type()) return false;
        
        switch (type()) {
            case ValueType::NIL: return true;
            case ValueType::BOOL: return asBool() == other.asBool();
            case ValueType::NUMBER: return asNumber() == other.asNumber();
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
        return Value(std::make_shared<ArrayType>(elements));
    }
    
    // Create function
    static Value makeFunction(const std::string& name,
                              const std::vector<std::string>& params,
                              const std::vector<StmtPtr>& body,
                              std::shared_ptr<Environment> closure) {
        return Value(std::make_shared<EZFunction>(name, params, body, closure));
    }
    
    // Create native function
    static Value makeNativeFunction(const std::string& name, int arity, NativeFn fn) {
        return Value(std::make_shared<NativeFunction>(name, arity, fn));
    }
    
    // Create dictionary
    static Value makeDictionary();
};

// EZ Class definition (model)
struct EZClass {
    std::string name;
    std::shared_ptr<EZClass> parent;
    std::vector<std::string> initParams;
    std::vector<StmtPtr> initBody;
    std::unordered_map<std::string, Value> methods;
    std::unordered_map<std::string, bool> visibility;  // true = public (shown)
    
    EZClass(const std::string& name) : name(name), parent(nullptr) {}
};

// EZ Instance (object created from model)
struct EZInstance {
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
};

struct EZDictionary {
    std::unordered_map<std::string, Value> map;
};

inline EZDictionary& Value::asDictionary() { return *std::get<DictionaryPtr>(data); }
inline const EZDictionary& Value::asDictionary() const { return *std::get<DictionaryPtr>(data); }
inline Value Value::makeDictionary() { return Value(std::make_shared<EZDictionary>()); }

// Implementations of Value methods that require complete types
inline std::string Value::toString() const {
    switch (type()) {
        case ValueType::NIL: return "nil";
        case ValueType::BOOL: return asBool() ? "true" : "false";
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
        default:
            return "<unknown>";
    }
}

inline std::string Value::typeName() const {
    switch (type()) {
        case ValueType::NIL: return "nil";
        case ValueType::BOOL: return "bool";
        case ValueType::NUMBER: return "number";
        case ValueType::STRING: return "string";
        case ValueType::ARRAY: return "array";
        case ValueType::FUNCTION: return "function";
        case ValueType::NATIVE_FUNCTION: return "function";
        case ValueType::CLASS: return "model";
        case ValueType::INSTANCE: return "instance";
        case ValueType::DICTIONARY: return "dictionary";
        default: return "unknown";
    }
}

#endif // VALUE_H
