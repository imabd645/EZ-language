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
#include "ast/AST.h"
#include "gc/CycleCollector.h"

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
    ATOMIC,
    TUPLE
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
struct EZTuple;

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
    using TuplePtr = std::shared_ptr<EZTuple>;

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
        AtomicPtr,          // ATOMIC
        TuplePtr            // TUPLE
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
    Value(TuplePtr val) : m_data(val) {}
    
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
            ValueType::ATOMIC,           // 20: AtomicPtr
            ValueType::TUPLE             // 21: TuplePtr
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
    bool isTuple() const { return std::holds_alternative<TuplePtr>(m_data); }
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
    TuplePtr asTuplePtr() const { try{return std::get<TuplePtr>(m_data);}catch(...){std::cerr<<"[Value] asTuplePtr fail, index="<<index()<<"\n";throw;} }

    // Convenience accessors for builtins
    EZArray& asArray();
    const EZArray& asArray() const;
    EZTuple& asTuple();
    const EZTuple& asTuple() const;
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
    static Value makeTuple(const std::vector<Value>& elements = {});
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

#include "objects/EZObjects.h"

#endif // VALUE_H
