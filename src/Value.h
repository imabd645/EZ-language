#ifndef VALUE_H
#define VALUE_H

#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <variant>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <cstring>
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
struct EZAtomic;

using NativeFn = std::function<Value(RuntimeContext&, const std::vector<Value>&)>;

enum class ValueType {
    NIL,
    BOOL,
    NUMBER,
    STRING,
    SHORT_STRING,
    CONCAT_STRING,
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
    INTERFACE,
    ATOMIC
};

struct ShortString {
    char data[14];
    uint8_t length;
    
    ShortString() : length(0) { data[0] = '\0'; }
    ShortString(const char* str, size_t len) {
        length = static_cast<uint8_t>(len);
        for(size_t i=0; i<len; i++) data[i] = str[i];
        if (len < 14) data[len] = '\0';
    }
    
    bool operator==(const ShortString& other) const {
        if (length != other.length) return false;
        for (size_t i = 0; i < length; i++) {
            if (data[i] != other.data[i]) return false;
        }
        return true;
    }
};

struct Value;
struct EZConcatString;

struct Value {
    // Pointer types for Variant
    using StringPtr = std::shared_ptr<std::string>;
    using ConcatStringPtr = std::shared_ptr<EZConcatString>;
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
    using AtomicPtr = std::shared_ptr<EZAtomic>;

    std::variant<
        std::nullptr_t,     // NIL
        bool,               // BOOL
        double,             // NUMBER
        StringPtr,          // STRING
        ShortString,        // SHORT_STRING
        ConcatStringPtr,    // CONCAT_STRING
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
        InterfacePtr,       // INTERFACE
        AtomicPtr           // ATOMIC
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
    Value(const std::string& val); // Defined in cpp or inline below
    Value(const char* val);        // Defined in cpp or inline below
    Value(StringPtr val) : m_data(val) {}
    Value(ShortString val) : m_data(val) {}
    Value(ConcatStringPtr val) : m_data(val) {}
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
    Value(AtomicPtr val) : m_data(val) {}
    
    // Type checking â€” O(1) via index lookup table
    // Table order must match the std::variant alternative order in m_data exactly.
    ValueType type() const {
        static constexpr ValueType typeTable[] = {
            ValueType::NIL,              // 0:  nullptr_t
            ValueType::BOOL,             // 1:  bool
            ValueType::NUMBER,           // 2:  double
            ValueType::STRING,           // 3:  StringPtr
            ValueType::SHORT_STRING,     // 4:  ShortString
            ValueType::CONCAT_STRING,    // 5:  ConcatStringPtr
            ValueType::ARRAY,            // 6:  ArrayPtr
            ValueType::FUNCTION,         // 7:  FunctionPtr
            ValueType::NATIVE_FUNCTION,  // 8:  NativeFnPtr
            ValueType::CLASS,            // 9:  ClassPtr
            ValueType::INSTANCE,         // 10: InstancePtr
            ValueType::DICTIONARY,       // 11: DictionaryPtr
            ValueType::FUTURE,           // 12: FuturePtr
            ValueType::SUPER,            // 13: SuperPtr
            ValueType::INTEGER,          // 14: long long
            ValueType::BUFFER,           // 15: BufferPtr
            ValueType::MUTEX,            // 16: MutexPtr
            ValueType::BOUND_METHOD,     // 17: BoundMethodPtr
            ValueType::CLOSURE_VAL,      // 18: ClosureValPtr
            ValueType::INTERFACE,        // 19: InterfacePtr
            ValueType::ATOMIC            // 20: AtomicPtr
        };
        return typeTable[m_data.index()];
    }
    
    bool isNil() const { return std::holds_alternative<std::nullptr_t>(m_data); }
    bool isBool() const { return std::holds_alternative<bool>(m_data); }
    bool isInteger() const { return std::holds_alternative<long long>(m_data); }
    bool isFloat() const { return std::holds_alternative<double>(m_data); }
    bool isNumber() const { return isInteger() || isFloat(); }
    bool isString() const { 
        return std::holds_alternative<StringPtr>(m_data) || 
               std::holds_alternative<ShortString>(m_data) || 
               std::holds_alternative<ConcatStringPtr>(m_data); 
    }
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
    bool isAtomic() const { return std::holds_alternative<AtomicPtr>(m_data); }
    bool isCallable() const { return isFunction() || isNativeFunction() || isClass() || isBoundMethod() || isClosure(); }
    
    // Value extraction
    size_t index() const { return m_data.index(); }
    
    bool asBool() const { 
        try { return std::get<bool>(m_data); }
        catch(...) { std::cerr << "[Value] asBool failed, index=" << index() << "\n"; throw; }
    }
    double asFloat() const { 
        if (index() == 14) return static_cast<double>(std::get<long long>(m_data)); 
        if (index() == 2)  return std::get<double>(m_data);
        std::cerr << "[Value] asFloat() failed: index=" << index() << std::endl;
        try { return std::get<double>(m_data); } catch(...) { throw; }
    }
    long long asInteger() const { 
        if (index() == 14) return std::get<long long>(m_data); 
        if (index() == 2)  return static_cast<long long>(std::get<double>(m_data)); 
        std::cerr << "[Value] asInteger() failed: index=" << index() << std::endl;
        try { return std::get<long long>(m_data); } catch(...) { throw; }
    }
    
    // Unsafe accessors (hot paths)
    long long asIntegerUnsafe() const { return std::get<long long>(m_data); }
    double asFloatUnsafe() const { return std::get<double>(m_data); }

    double asNumber() const { return asFloat(); }
    size_t stringLength() const;
    StringPtr asStringPtr() const;
    std::string asString() const;
    ConcatStringPtr asConcatStringPtr() const { try{return std::get<ConcatStringPtr>(m_data);}catch(...){std::cerr<<"[Value] asConcatStringPtr fail, index="<<index()<<"\n";throw;} }
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
    AtomicPtr asAtomicPtr() const { try{return std::get<AtomicPtr>(m_data);}catch(...){std::cerr<<"[Value] asAtomicPtr fail, index="<<index()<<"\n";throw;} }

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
    static Value makeAtomic(long long initial);
};

struct EZConcatString : public GCObject {
    Value left;
    Value right;
    size_t length = 0;
    bool isFlattened = false;
    std::shared_ptr<std::string> flattened;
    
    void traverse(GCObjectVisitor& visitor) override;
    void gc_clear() override {
        left = Value();
        right = Value();
        flattened = nullptr;
    }
};

// --- GCObject-derived structs that use Value ---

struct EZArray : public GCObject {
    std::vector<Value> elements;
    EZArray(const std::vector<Value>& e = {}) : elements(e) {}
    void traverse(GCObjectVisitor& visitor) override;
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
    void traverse(GCObjectVisitor& visitor) override;
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

    void traverse(GCObjectVisitor& visitor) override;
    void gc_clear() override { closure = nullptr; staticEnv = nullptr; bytecode = nullptr; }
};

struct NativeFunction {
    std::string name;
    int arity;
    NativeFn function;
    NativeFunction(const std::string& name, int arity, NativeFn fn)
        : name(name), arity(arity), function(fn) {}
};

// â”€â”€ Behavior flags â€” one bit per active decorator â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
struct BehaviorFlags {
    bool audited    : 1;
    bool snapshot   : 1;
    bool persistent : 1;
    bool validated  : 1;
    bool hasCached  : 1;
    bool any() const { return audited || snapshot || persistent || validated || hasCached; }
};

// Per-field validator (lives on EZClass)
struct FieldValidator {
    std::string field;
    std::string rule;    // "minlen","maxlen","min","max","email","pattern","notnull"
    Value       param;   // Value::NIL for rules without params
    std::string message;
};

// Audit entry (lives in EZInstance's auditLog)
struct AuditEntry {
    std::string field;
    Value       oldValue;
    Value       newValue;
    std::string via;       // calling task name
    long long   timestamp; // ms since epoch
};

// Cached method result (lives in EZInstance's cacheStore)
struct CachedResult {
    Value                            result;
    std::unordered_set<std::string>  deps;    // self fields read during computation
    bool                             dirty = true;
};

struct EZClass : public GCObject {
    std::string name;
    std::shared_ptr<EZClass> parent;
    std::unordered_map<std::string, Value> methods;
    std::unordered_map<std::string, Value> staticMembers;
    std::unordered_map<std::string, bool>  visibility;
    
    // Legacy support for AST Interpreter
    std::vector<std::string> initParams;
    std::vector<StmtPtr> initBody;

    // â”€â”€ Decorator metadata â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    BehaviorFlags behaviors = {false,false,false,false,false};
    std::string persistPath;
    std::vector<FieldValidator> validators;
    std::unordered_set<std::string> cachedMethods;

    EZClass(const std::string& name) : name(name), parent(nullptr) {}
    void traverse(GCObjectVisitor& visitor) override;
    void gc_clear() override {
        parent = nullptr;
        methods.clear();
        staticMembers.clear();
        for (auto& v : validators) v.param = Value();
    }
};

struct EZInstance : public GCObject {
    std::shared_ptr<EZClass> klass;
    std::unordered_map<std::string, Value> properties;
    mutable std::shared_mutex prop_mutex; // protects properties for concurrent access

    // â”€â”€ Decorator runtime state (lazily allocated) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    std::vector<AuditEntry>*                       auditLog   = nullptr;
    std::unordered_map<std::string, CachedResult>* cacheStore = nullptr;

    EZInstance(std::shared_ptr<EZClass> klass) : klass(klass) {}
    ~EZInstance() {
        delete auditLog;
        delete cacheStore;
    }
    void traverse(GCObjectVisitor& visitor) override;
    void gc_clear() override {
        std::unique_lock<std::shared_mutex> lk(prop_mutex);
        properties.clear();
        klass = nullptr;
        if (auditLog)   auditLog->clear();
        if (cacheStore) cacheStore->clear();
    }
    
    Value getProperty(const std::string& name) {
        // First check instance properties (shared read)
        {
            std::shared_lock<std::shared_mutex> lk(prop_mutex);
            auto it = properties.find(name);
            if (it != properties.end()) return it->second;
        }
        // Then search class hierarchy (read-only, no lock needed â€” class methods are set once)
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
        
    void traverse(GCObjectVisitor& visitor) override {}
    void gc_clear() override { requiredMethods.clear(); }
};

struct EZBoundMethod : public GCObject {
    Value receiver;
    Value method;
    EZBoundMethod(const Value& receiver, const Value& method)
        : receiver(receiver), method(method) {}
    void traverse(GCObjectVisitor& visitor) override;
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
    void traverse(GCObjectVisitor& visitor) override;
    void gc_clear() override;
};

struct EZBuffer : public GCObject {
    std::vector<uint8_t> data;
    EZBuffer(size_t size = 0) : data(size) {}
    EZBuffer(const std::vector<uint8_t>& d) : data(d) {}
    void traverse(GCObjectVisitor& visitor) override {}
    void gc_clear() override { data.clear(); }
    size_t size() const { return data.size(); }
};

struct EZMutex : public GCObject {
    std::recursive_mutex mtx;
    void traverse(GCObjectVisitor& visitor) override {}
    void gc_clear() override {}
    void lock() { mtx.lock(); }
    void unlock() { mtx.unlock(); }
};

struct EZAtomic : public GCObject {
    std::atomic<long long> val;
    EZAtomic(long long initial = 0) : val(initial) {}
    void traverse(GCObjectVisitor& visitor) override {}
    void gc_clear() override {}
};

// --- Value Method Implementations (at the end for type completion) ---

inline EZArray& Value::asArray() { try{return *std::get<ArrayPtr>(m_data);}catch(...){std::cerr<<"[Value] inline asArray fail, index="<<index()<<"\n";throw;} }
inline const EZArray& Value::asArray() const { try{return *std::get<ArrayPtr>(m_data);}catch(...){std::cerr<<"[Value] inline asArray const fail, index="<<index()<<"\n";throw;} }
inline EZDictionary& Value::asDictionary() { try{return *std::get<DictionaryPtr>(m_data);}catch(...){std::cerr<<"[Value] inline asDictionary fail, index="<<index()<<"\n";throw;} }
inline const EZDictionary& Value::asDictionary() const { try{return *std::get<DictionaryPtr>(m_data);}catch(...){std::cerr<<"[Value] inline asDictionary const fail, index="<<index()<<"\n";throw;} }
inline std::vector<uint8_t>& Value::asBuffer() { try{return std::get<BufferPtr>(m_data)->data;}catch(...){std::cerr<<"[Value] inline asBuffer fail, index="<<index()<<"\n";throw;} }
inline std::vector<uint8_t>& Value::asBuffer() const { try{return std::get<BufferPtr>(m_data)->data;}catch(...){std::cerr<<"[Value] inline asBuffer const fail, index="<<index()<<"\n";throw;} }

// --- Value String Implementations ---
extern thread_local std::unordered_map<std::string, std::weak_ptr<std::string>> globalStringPool;

inline Value::Value(const std::string& val) {
    if (val.length() < 14) {
        m_data = ShortString(val.c_str(), val.length());
    } else {
        auto it = globalStringPool.find(val);
        if (it != globalStringPool.end() && !it->second.expired()) {
            m_data = it->second.lock();
        } else {
            auto ptr = std::make_shared<std::string>(val);
            globalStringPool[val] = ptr;
            m_data = ptr;
        }
    }
}

inline Value::Value(const char* val) {
    size_t len = std::strlen(val);
    if (len < 14) {
        m_data = ShortString(val, len);
    } else {
        std::string sval(val, len);
        auto it = globalStringPool.find(sval);
        if (it != globalStringPool.end() && !it->second.expired()) {
            m_data = it->second.lock();
        } else {
            auto ptr = std::make_shared<std::string>(std::move(sval));
            globalStringPool[*ptr] = ptr;
            m_data = ptr;
        }
    }
}

inline std::shared_ptr<std::string> flattenConcatString(const Value::ConcatStringPtr& rootCs) {
    if (rootCs->isFlattened) {
        return rootCs->flattened;
    }
    
    auto result = std::make_shared<std::string>();
    result->resize(rootCs->length);
    char* dest = &(*result)[0];
    size_t offset = 0;
    
    std::vector<Value> stack;
    stack.reserve(128);
    stack.push_back(Value(rootCs));
    
    while (!stack.empty()) {
        Value val = stack.back();
        stack.pop_back();
        
        if (val.index() == 5) { // CONCAT_STRING
            auto cs = std::get<Value::ConcatStringPtr>(val.m_data);
            if (cs->isFlattened) {
                std::memcpy(dest + offset, cs->flattened->data(), cs->flattened->length());
                offset += cs->flattened->length();
            } else {
                stack.push_back(cs->right);
                stack.push_back(cs->left);
            }
        } else if (val.index() == 3) { // STRING
            const auto& s = *std::get<Value::StringPtr>(val.m_data);
            std::memcpy(dest + offset, s.data(), s.length());
            offset += s.length();
        } else if (val.index() == 4) { // SHORT_STRING
            const auto& ss = std::get<ShortString>(val.m_data);
            std::memcpy(dest + offset, ss.data, ss.length);
            offset += ss.length;
        }
    }
    
    rootCs->flattened = result;
    rootCs->isFlattened = true;
    return result;
}

inline Value::StringPtr Value::asStringPtr() const {
    if (index() == 3) return std::get<StringPtr>(m_data);
    if (index() == 4) {
        const auto& ss = std::get<ShortString>(m_data);
        return std::make_shared<std::string>(ss.data, ss.length);
    }
    if (index() == 5) {
        const auto& cs = std::get<ConcatStringPtr>(m_data);
        return flattenConcatString(cs);
    }
    std::cerr << "[Value] asStringPtr fail, index=" << index() << "\n"; throw std::runtime_error("Not a string");
}

inline size_t Value::stringLength() const {
    if (index() == 3) return std::get<StringPtr>(m_data)->length();
    if (index() == 4) return std::get<ShortString>(m_data).length;
    if (index() == 5) return std::get<ConcatStringPtr>(m_data)->length;
    std::cerr << "[Value] stringLength fail, index=" << index() << "\n"; throw std::runtime_error("Not a string");
}

inline std::string Value::asString() const {
    if (index() == 3) return *std::get<StringPtr>(m_data);
    if (index() == 4) {
        const auto& ss = std::get<ShortString>(m_data);
        return std::string(ss.data, ss.length);
    }
    if (index() == 5) {
        const auto& cs = std::get<ConcatStringPtr>(m_data);
        return *flattenConcatString(cs);
    }
    std::cerr << "[Value] asString fail, index=" << index() << "\n"; throw std::runtime_error("Not a string");
}

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
        case ValueType::SHORT_STRING: return asString();
        case ValueType::CONCAT_STRING: return asString();
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
    if (isString() && other.isString()) {
        if (stringLength() != other.stringLength()) return false;
        return asString() == other.asString();
    }
    if (type() != other.type()) return false;
    switch (type()) {
        case ValueType::NIL: return true;
        case ValueType::BOOL: return asBool() == other.asBool();
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
inline Value Value::makeAtomic(long long initial) { return Value(std::make_shared<EZAtomic>(initial)); }

#endif // VALUE_H
