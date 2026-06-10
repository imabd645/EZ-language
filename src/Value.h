#ifndef VALUE_H
#define VALUE_H

#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <variant>
#include <functional>
#include <unordered_map>
#include <future>
#include <mutex>
#include <shared_mutex>
#include "AST.h"
#include "GCObject.h"

// Forward-declare EZFuture (defined in EZFuture.h / EZFuture.cpp)
// Only files that actually construct or await futures need to include EZFuture.h.
struct EZFuture;

class RuntimeContext;
class Environment;
struct Value;

using ValuePtr = std::shared_ptr<Value>;

// Forward declarations
struct EZArray;
struct EZFunction;
struct NativeFunction;
struct EZClass;
struct EZInstance;
struct EZDictionary;
struct EZSuper;
struct EZBoundMethod;
struct EZBuffer;
struct EZMutex;
struct EZClosure;
struct UpvalueObj;

using NativeFn = std::function<Value(RuntimeContext&, const std::vector<Value>&)>;

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
    BUFFER,
    MUTEX,
    BOUND_METHOD,
    CLOSURE_VAL,
    INTERFACE
};

struct Value {
    // Pointer types for Variant
    using StringPtr = std::shared_ptr<std::string>;
    using ArrayPtr = std::shared_ptr<EZArray>;
    using FunctionPtr = std::shared_ptr<EZFunction>;
    using NativeFnPtr = std::shared_ptr<NativeFunction>;
    using ClassPtr = std::shared_ptr<EZClass>;
    using InstancePtr = std::shared_ptr<EZInstance>;
    using DictionaryPtr = std::shared_ptr<EZDictionary>;
    using FuturePtr = std::shared_ptr<EZFuture>;
    using SuperPtr = std::shared_ptr<EZSuper>;
    using BufferPtr = std::shared_ptr<EZBuffer>;
    using MutexPtr = std::shared_ptr<EZMutex>;
    using BoundMethodPtr = std::shared_ptr<EZBoundMethod>;
    using ClosureValPtr = std::shared_ptr<EZClosure>;
    using InterfacePtr = std::shared_ptr<struct EZInterface>;

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
        BufferPtr,          // BUFFER
        MutexPtr,           // MUTEX
        BoundMethodPtr,     // BOUND_METHOD
        ClosureValPtr,      // CLOSURE_VAL
        InterfacePtr        // INTERFACE
    > m_data;
    
    // Constructors
    Value() : m_data(nullptr) {}
    Value(std::nullptr_t) : m_data(nullptr) {}
    Value(bool val) : m_data(val) {}
    Value(double val) : m_data(val) {}
    Value(int val) : m_data(static_cast<long long>(val)) {}
    Value(unsigned int val) : m_data(static_cast<long long>(val)) {}
    Value(long val) : m_data(static_cast<long long>(val)) {}
    Value(unsigned long val) : m_data(static_cast<long long>(val)) {}
    Value(long long val) : m_data(val) {}
    Value(unsigned long long val) : m_data(static_cast<long long>(val)) {}
    Value(const std::string& val) : m_data(std::make_shared<std::string>(val)) {}
    Value(const char* val) : m_data(std::make_shared<std::string>(val)) {}
    Value(StringPtr val) : m_data(val) {}
    Value(ArrayPtr val) : m_data(val) {}
    Value(FunctionPtr val) : m_data(val) {}
    Value(NativeFnPtr val) : m_data(val) {}
    Value(ClassPtr val) : m_data(val) {}
    Value(InstancePtr val) : m_data(val) {}
    Value(DictionaryPtr val) : m_data(val) {}
    Value(FuturePtr val) : m_data(val) {}
    Value(SuperPtr val) : m_data(val) {}
    Value(BoundMethodPtr val) : m_data(val) {}
    Value(BufferPtr val) : m_data(val) {}
    Value(MutexPtr val) : m_data(val) {}
    Value(ClosureValPtr val) : m_data(val) {}
    Value(InterfacePtr val) : m_data(val) {}
    
    // Type checking — O(1) via index lookup table
    // Table order must match the std::variant alternative order in m_data exactly.
    ValueType type() const {
        static constexpr ValueType typeTable[] = {
            ValueType::NIL,              // 0:  nullptr_t
            ValueType::BOOL,             // 1:  bool
            ValueType::NUMBER,           // 2:  double
            ValueType::STRING,           // 3:  StringPtr
            ValueType::ARRAY,            // 4:  ArrayPtr
            ValueType::FUNCTION,         // 5:  FunctionPtr
            ValueType::NATIVE_FUNCTION,  // 6:  NativeFnPtr
            ValueType::CLASS,            // 7:  ClassPtr
            ValueType::INSTANCE,         // 8:  InstancePtr
            ValueType::DICTIONARY,       // 9:  DictionaryPtr
            ValueType::FUTURE,           // 10: FuturePtr
            ValueType::SUPER,            // 11: SuperPtr
            ValueType::INTEGER,          // 12: long long
            ValueType::BUFFER,           // 13: BufferPtr
            ValueType::MUTEX,            // 14: MutexPtr
            ValueType::BOUND_METHOD,     // 15: BoundMethodPtr
            ValueType::CLOSURE_VAL,      // 16: ClosureValPtr
            ValueType::INTERFACE         // 17: InterfacePtr
        };
        return typeTable[m_data.index()];
    }
    
    bool isNil() const { return std::holds_alternative<std::nullptr_t>(m_data); }
    bool isBool() const { return std::holds_alternative<bool>(m_data); }
    bool isInteger() const { return std::holds_alternative<long long>(m_data); }
    bool isFloat() const { return std::holds_alternative<double>(m_data); }
    bool isNumber() const { return isInteger() || isFloat(); }
    bool isString() const { return std::holds_alternative<StringPtr>(m_data); }
    bool isArray() const { return std::holds_alternative<ArrayPtr>(m_data); }
    bool isFunction() const { return std::holds_alternative<FunctionPtr>(m_data); }
    bool isNativeFunction() const { return std::holds_alternative<NativeFnPtr>(m_data); }
    bool isClass() const { return std::holds_alternative<ClassPtr>(m_data); }
    bool isInstance() const { return std::holds_alternative<InstancePtr>(m_data); }
    bool isDictionary() const { return std::holds_alternative<DictionaryPtr>(m_data); }
    bool isFuture() const { return std::holds_alternative<FuturePtr>(m_data); }
    bool isSuper() const { return std::holds_alternative<SuperPtr>(m_data); }
    bool isBuffer() const { return std::holds_alternative<BufferPtr>(m_data); }
    bool isMutex() const { return std::holds_alternative<MutexPtr>(m_data); }
    bool isBoundMethod() const { return std::holds_alternative<BoundMethodPtr>(m_data); }
    bool isClosure() const { return std::holds_alternative<ClosureValPtr>(m_data); }
    bool isInterface() const { return std::holds_alternative<InterfacePtr>(m_data); }
    bool isCallable() const { return isFunction() || isNativeFunction() || isClass() || isBoundMethod() || isClosure(); }
    
    // Value extraction
    size_t index() const { return m_data.index(); }
    
    bool asBool() const { 
        try { return std::get<bool>(m_data); }
        catch(...) { std::cerr << "[Value] asBool failed, index=" << index() << "\n"; throw; }
    }
    double asFloat() const { 
        if (index() == 12) return static_cast<double>(std::get<long long>(m_data)); 
        if (index() == 2)  return std::get<double>(m_data);
        std::cerr << "[Value] asFloat() failed: index=" << index() << std::endl;
        try { return std::get<double>(m_data); } catch(...) { throw; }
    }
    long long asInteger() const { 
        if (index() == 12) return std::get<long long>(m_data); 
        if (index() == 2)  return static_cast<long long>(std::get<double>(m_data)); 
        std::cerr << "[Value] asInteger() failed: index=" << index() << std::endl;
        try { return std::get<long long>(m_data); } catch(...) { throw; }
    }
    
    // Unsafe accessors (hot paths)
    long long asIntegerUnsafe() const { return std::get<long long>(m_data); }
    double asFloatUnsafe() const { return std::get<double>(m_data); }

    double asNumber() const { return asFloat(); }
    StringPtr asStringPtr() const { try{return std::get<StringPtr>(m_data);}catch(...){std::cerr<<"[Value] asStringPtr fail, index="<<index()<<"\n";throw;} }
    const std::string& asString() const { try{return *std::get<StringPtr>(m_data);}catch(...){std::cerr<<"[Value] asString fail, index="<<index()<<"\n";throw;} }
    ArrayPtr asArrayPtr() const { try{return std::get<ArrayPtr>(m_data);}catch(...){std::cerr<<"[Value] asArrayPtr fail, index="<<index()<<"\n";throw;} }
    FunctionPtr asFunction() const { try{return std::get<FunctionPtr>(m_data);}catch(...){std::cerr<<"[Value] asFunction fail, index="<<index()<<"\n";throw;} }
    NativeFnPtr asNativeFunction() const { try{return std::get<NativeFnPtr>(m_data);}catch(...){std::cerr<<"[Value] asNativeFunction fail, index="<<index()<<"\n";throw;} }
    ClassPtr asClass() const { try{return std::get<ClassPtr>(m_data);}catch(...){std::cerr<<"[Value] asClass fail, index="<<index()<<"\n";throw;} }
    InstancePtr asInstance() const { try{return std::get<InstancePtr>(m_data);}catch(...){std::cerr<<"[Value] asInstance fail, index="<<index()<<"\n";throw;} }
    DictionaryPtr asDictionaryPtr() const { try{return std::get<DictionaryPtr>(m_data);}catch(...){std::cerr<<"[Value] asDictionaryPtr fail, index="<<index()<<"\n";throw;} }
    FuturePtr asFuture() const { try{return std::get<FuturePtr>(m_data);}catch(...){std::cerr<<"[Value] asFuture fail, index="<<index()<<"\n";throw;} }
    SuperPtr asSuper() const { try{return std::get<SuperPtr>(m_data);}catch(...){std::cerr<<"[Value] asSuper fail, index="<<index()<<"\n";throw;} }
    InterfacePtr asInterface() const { try{return std::get<InterfacePtr>(m_data);}catch(...){std::cerr<<"[Value] asInterface fail, index="<<index()<<"\n";throw;} }
    BoundMethodPtr asBoundMethod() const { try{return std::get<BoundMethodPtr>(m_data);}catch(...){std::cerr<<"[Value] asBoundMethod fail, index="<<index()<<"\n";throw;} }
    ClosureValPtr asClosure() const { try{return std::get<ClosureValPtr>(m_data);}catch(...){std::cerr<<"[Value] asClosure fail, index="<<index()<<"\n";throw;} }
    BufferPtr asBufferPtr() const { try{return std::get<BufferPtr>(m_data);}catch(...){std::cerr<<"[Value] asBufferPtr fail, index="<<index()<<"\n";throw;} }
    MutexPtr asMutexPtr() const { try{return std::get<MutexPtr>(m_data);}catch(...){std::cerr<<"[Value] asMutexPtr fail, index="<<index()<<"\n";throw;} }

    // Convenience accessors for builtins
    EZArray& asArray();
    const EZArray& asArray() const;
    EZDictionary& asDictionary();
    const EZDictionary& asDictionary() const;
    std::vector<uint8_t>& asBuffer();
    std::vector<uint8_t>& asBuffer() const;
    
    bool isTruthy() const {
        if (isNil()) return false;
        if (isBool()) return asBool();
        return true;
    }
    
    bool equals(const Value& other) const;
    std::string toString() const;
    std::string typeName() const;
    
    static Value makeArray(const std::vector<Value>& elements = {});
    static Value makeArrayCopy(const EZArray& other);
    static Value makeFunction(const std::string& name,
                              const std::vector<std::string>& params,
                              const std::vector<ExprPtr>& defaultValues,
                              const std::vector<StmtPtr>& body,
                              std::shared_ptr<Environment> closure,
                              bool variadic = false);
    static Value makeNativeFunction(const std::string& name, int arity, NativeFn fn);
    static Value makeDictionary();
    static Value makeFuture(std::shared_ptr<EZFuture> fut);
    static Value makeSuper(InstancePtr instance, ClassPtr parentKlass);
    static Value makeClosure(ClosureValPtr closure);
};

// --- GCObject-derived structs that use Value ---

struct EZArray : public GCObject {
    std::vector<Value> elements;
    EZArray(const std::vector<Value>& e = {}) : elements(e) {}
    void gc_mark() override;
    void gc_clear() override { elements.clear(); }
    
    size_t size() const { return elements.size(); }
    bool empty() const { return elements.empty(); }
    void push_back(const Value& v) { elements.push_back(v); }
    void pop_back() { elements.pop_back(); }
    void resize(size_t newSize) { elements.resize(newSize); }
    Value& back() { return elements.back(); }
    Value& operator[](size_t i) { return elements[i]; }
    const Value& operator[](size_t i) const { return elements[i]; }
    auto begin() { return elements.begin(); }
    auto end() { return elements.end(); }
    auto begin() const { return elements.begin(); }
    auto end() const { return elements.end(); }
    void erase(std::vector<Value>::iterator it) { elements.erase(it); }
    void insert(std::vector<Value>::iterator it, const Value& v) { elements.insert(it, v); }
};

struct EZDictionary : public GCObject {
    std::unordered_map<std::string, Value> map;
    mutable std::shared_mutex map_mutex;
    void gc_mark() override;
    void gc_clear() override {
        std::unique_lock<std::shared_mutex> lk(map_mutex);
        map.clear();
    }
};

struct EZFunction : public GCObject {
    std::string name;
    std::vector<std::string> params;
    std::vector<ExprPtr> defaultValues;
    std::vector<StmtPtr> body;
    std::shared_ptr<Environment> closure;
    std::shared_ptr<Environment> staticEnv;
    bool isVariadic;
    std::shared_ptr<struct BytecodeFunction> bytecode;
    
    EZFunction(const std::string& name, 
               const std::vector<std::string>& params,
               const std::vector<ExprPtr>& defaultValues,
               const std::vector<StmtPtr>& body,
               std::shared_ptr<Environment> closure,
               bool variadic = false)
        : name(name), params(params), defaultValues(defaultValues), body(body), 
          closure(closure), isVariadic(variadic) {}

    void gc_mark() override;
    void gc_clear() override { closure = nullptr; staticEnv = nullptr; bytecode = nullptr; }
};

struct NativeFunction {
    std::string name;
    int arity;
    NativeFn function;
    NativeFunction(const std::string& name, int arity, NativeFn fn)
        : name(name), arity(arity), function(fn) {}
};

struct EZClass : public GCObject {
    std::string name;
    std::shared_ptr<EZClass> parent;
    std::unordered_map<std::string, Value> methods;
    std::unordered_map<std::string, Value> staticMembers;
    std::unordered_map<std::string, bool> visibility;
    
    // Legacy support for AST Interpreter
    std::vector<std::string> initParams;
    std::vector<StmtPtr> initBody;

    EZClass(const std::string& name) : name(name), parent(nullptr) {}
    void gc_mark() override;
    void gc_clear() override { parent = nullptr; methods.clear(); staticMembers.clear(); }
};

struct EZInstance : public GCObject {
    std::shared_ptr<EZClass> klass;
    std::unordered_map<std::string, Value> properties;
    mutable std::shared_mutex prop_mutex; // protects properties for concurrent access
    EZInstance(std::shared_ptr<EZClass> klass) : klass(klass) {}
    void gc_mark() override;
    void gc_clear() override {
        std::unique_lock<std::shared_mutex> lk(prop_mutex);
        properties.clear(); klass = nullptr;
    }
    
    Value getProperty(const std::string& name) {
        // First check instance properties (shared read)
        {
            std::shared_lock<std::shared_mutex> lk(prop_mutex);
            auto it = properties.find(name);
            if (it != properties.end()) return it->second;
        }
        // Then search class hierarchy (read-only, no lock needed — class methods are set once)
        std::shared_ptr<EZClass> currentClass = klass;
        while (currentClass) {
            if (currentClass->methods.count(name)) return currentClass->methods[name];
            currentClass = currentClass->parent;
        }
        return Value();
    }
    void setProperty(const std::string& name, const Value& value) {
        std::unique_lock<std::shared_mutex> lk(prop_mutex);
        properties[name] = value;
    }
    bool hasProperty(const std::string& name) {
        std::shared_lock<std::shared_mutex> lk(prop_mutex);
        return properties.count(name) > 0;
    }
};

struct EZSuper {
    std::shared_ptr<EZInstance> instance;
    std::shared_ptr<EZClass> parentKlass;
    EZSuper(std::shared_ptr<EZInstance> instance, std::shared_ptr<EZClass> parentKlass) 
        : instance(instance), parentKlass(parentKlass) {}
};

struct EZInterface : public GCObject {
    std::string name;
    std::vector<std::string> requiredMethods;
    
    EZInterface(const std::string& name, const std::vector<std::string>& methods)
        : name(name), requiredMethods(methods) {}
        
    void gc_mark() override {}
    void gc_clear() override { requiredMethods.clear(); }
};

struct EZBoundMethod : public GCObject {
    Value receiver;
    Value method;
    EZBoundMethod(const Value& receiver, const Value& method)
        : receiver(receiver), method(method) {}
    void gc_mark() override;
    void gc_clear() override;
};

#include <atomic>

struct UpvalueObj {
    std::atomic<Value*> location;   // Points to stack slot (open) or &closed (closed)
    Value  closed;     // When closed, location == &closed
    UpvalueObj* next;
};

struct EZClosure : public GCObject {
    std::shared_ptr<struct BytecodeFunction> function;
    std::vector<UpvalueObj*> upvalues;
    EZClosure(std::shared_ptr<struct BytecodeFunction> f) : function(f) {}
    void gc_mark() override;
    void gc_clear() override;
};

struct EZBuffer : public GCObject {
    std::vector<uint8_t> data;
    EZBuffer(size_t size = 0) : data(size) {}
    EZBuffer(const std::vector<uint8_t>& d) : data(d) {}
    void gc_mark() override { gc_marked = true; }
    void gc_clear() override { data.clear(); }
    size_t size() const { return data.size(); }
};

struct EZMutex : public GCObject {
    std::recursive_mutex mtx;
    void gc_mark() override { gc_marked = true; }
    void gc_clear() override {}
    void lock() { mtx.lock(); }
    void unlock() { mtx.unlock(); }
};

// --- Value Method Implementations (at the end for type completion) ---

inline EZArray& Value::asArray() { try{return *std::get<ArrayPtr>(m_data);}catch(...){std::cerr<<"[Value] inline asArray fail, index="<<index()<<"\n";throw;} }
inline const EZArray& Value::asArray() const { try{return *std::get<ArrayPtr>(m_data);}catch(...){std::cerr<<"[Value] inline asArray const fail, index="<<index()<<"\n";throw;} }
inline EZDictionary& Value::asDictionary() { try{return *std::get<DictionaryPtr>(m_data);}catch(...){std::cerr<<"[Value] inline asDictionary fail, index="<<index()<<"\n";throw;} }
inline const EZDictionary& Value::asDictionary() const { try{return *std::get<DictionaryPtr>(m_data);}catch(...){std::cerr<<"[Value] inline asDictionary const fail, index="<<index()<<"\n";throw;} }
inline std::vector<uint8_t>& Value::asBuffer() { try{return std::get<BufferPtr>(m_data)->data;}catch(...){std::cerr<<"[Value] inline asBuffer fail, index="<<index()<<"\n";throw;} }
inline std::vector<uint8_t>& Value::asBuffer() const { try{return std::get<BufferPtr>(m_data)->data;}catch(...){std::cerr<<"[Value] inline asBuffer const fail, index="<<index()<<"\n";throw;} }

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
        case ValueType::FUNCTION: return "<function>";
        case ValueType::NATIVE_FUNCTION: return "<native fn>";
        case ValueType::CLASS: return "<model>";
        case ValueType::INSTANCE: return "<instance>";
        case ValueType::DICTIONARY: return "<dictionary>";
        case ValueType::FUTURE: return "<future>";
        case ValueType::BOUND_METHOD: return "<bound method>";
        case ValueType::CLOSURE_VAL: return "<function>";
        case ValueType::INTERFACE: return "<interface " + asInterface()->name + ">";
        case ValueType::MUTEX: return "<mutex>";
        default: return "<unknown>";
    }
}

inline std::string Value::typeName() const {
    switch (type()) {
        case ValueType::NIL: return "nil";
        case ValueType::BOOL: return "bool";
        case ValueType::INTEGER: return "integer";
        case ValueType::NUMBER: return "float";
        case ValueType::STRING: return "string";
        case ValueType::ARRAY: return "array";
        case ValueType::FUNCTION: return "function";
        case ValueType::NATIVE_FUNCTION: return "function";
        case ValueType::CLASS: return "model";
        case ValueType::INSTANCE: return "instance";
        case ValueType::DICTIONARY: return "dictionary";
        case ValueType::FUTURE: return "future";
        case ValueType::BUFFER: return "buffer";
        case ValueType::MUTEX: return "mutex";
        case ValueType::BOUND_METHOD: return "function";
        case ValueType::SUPER: return "super";
        case ValueType::CLOSURE_VAL: return "function";
        case ValueType::INTERFACE: return "interface";
        default: return "unknown";
    }
}

inline bool Value::equals(const Value& other) const {
    if (isNumber() && other.isNumber()) {
        return asNumber() == other.asNumber();
    }
    if (type() != other.type()) return false;
    switch (type()) {
        case ValueType::NIL: return true;
        case ValueType::BOOL: return asBool() == other.asBool();
        case ValueType::STRING: return asString() == other.asString();
        default: return m_data == other.m_data;
    }
}

inline Value Value::makeArray(const std::vector<Value>& elements) {
    return Value(std::make_shared<EZArray>(elements));
}

inline Value Value::makeArrayCopy(const EZArray& other) {
    return Value(std::make_shared<EZArray>(other.elements));
}

inline Value Value::makeFunction(const std::string& name,
                          const std::vector<std::string>& params,
                          const std::vector<ExprPtr>& defaultValues,
                          const std::vector<StmtPtr>& body,
                          std::shared_ptr<Environment> closure,
                          bool variadic) {
    return Value(std::make_shared<EZFunction>(name, params, defaultValues, body, closure, variadic));
}

inline Value Value::makeNativeFunction(const std::string& name, int arity, NativeFn fn) {
    return Value(std::make_shared<NativeFunction>(name, arity, fn));
}

inline Value Value::makeDictionary() { return Value(std::make_shared<EZDictionary>()); }
inline Value Value::makeSuper(InstancePtr instance, ClassPtr parentKlass) { return Value(std::make_shared<EZSuper>(instance, parentKlass)); }
inline Value Value::makeFuture(std::shared_ptr<EZFuture> fut) { return Value(fut); }
inline Value Value::makeClosure(ClosureValPtr closure) { return Value(closure); }

#endif // VALUE_H
