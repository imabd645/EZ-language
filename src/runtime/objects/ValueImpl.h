#ifndef VALUEIMPL_H
#define VALUEIMPL_H
#include <limits>

// --- Value Method Implementations (at the end for type completion) ---

inline EZArray& Value::asArray() { try{return *std::get<ArrayPtr>(m_data);}catch(...){std::cerr<<"[Value] inline asArray fail, index="<<index()<<"\n";throw;} }
inline const EZArray& Value::asArray() const { try{return *std::get<ArrayPtr>(m_data);}catch(...){std::cerr<<"[Value] inline asArray const fail, index="<<index()<<"\n";throw;} }
inline EZTuple& Value::asTuple() { try{return *std::get<TuplePtr>(m_data);}catch(...){std::cerr<<"[Value] inline asTuple fail, index="<<index()<<"\n";throw;} }
inline const EZTuple& Value::asTuple() const { try{return *std::get<TuplePtr>(m_data);}catch(...){std::cerr<<"[Value] inline asTuple const fail, index="<<index()<<"\n";throw;} }
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
    if (!val) {
        return;
    }
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
            if (num >= static_cast<double>(std::numeric_limits<long long>::min()) &&
                num <= static_cast<double>(std::numeric_limits<long long>::max()) &&
                num == static_cast<long long>(num)) {
                return std::to_string(static_cast<long long>(num));
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
        case ValueType::TUPLE: {
            std::string result = "(";
            const auto& tup = asTuple();
            for (size_t i = 0; i < tup.size(); i++) {
                if (i > 0) result += ", ";
                if (tup[i].isString()) {
                    result += "\"" + tup[i].toString() + "\"";
                } else {
                    result += tup[i].toString();
                }
            }
            result += ")";
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
        case ValueType::CHANNEL: return "<channel>";
        default: return "<unknown>";
    }
}

inline std::string Value::typeName() const {
    switch (type()) {
        case ValueType::NIL: return "nil";
        case ValueType::BOOL: return "bool";
        case ValueType::INTEGER: return "integer";
        case ValueType::NUMBER: return "float";
        // A string has three runtime representations: a heap STRING, an inline
        // SHORT_STRING (<=14 chars) and a lazy CONCAT_STRING. Only STRING was
        // listed here, so typeOf() answered "unknown" for any short or
        // concatenated string -- i.e. for most strings in practice. That silently
        // broke value dispatch in EZ code: lib/db.ez binds a parameter with
        // `when typeOf(val) == "string"`, so text params were never bound and the
        // column stored NULL (reads then came back nil -> "Name mismatch").
        case ValueType::STRING:
        case ValueType::SHORT_STRING:
        case ValueType::CONCAT_STRING: return "string";
        case ValueType::ARRAY: return "array";
        case ValueType::TUPLE: return "tuple";
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
        case ValueType::CHANNEL: return "channel";
        case ValueType::ATOMIC: return "atomic";
        default: return "unknown";
    }
}

inline bool Value::equals(const Value& other) const {
    if (isNumber() && other.isNumber()) {
        return asNumber() == other.asNumber();
    }
    if (isString() && other.isString()) {
        if (stringLength() != other.stringLength()) return false;
        if (index() == 4 && other.index() == 4) { // SHORT_STRING fast path
            return std::get<ShortString>(m_data) == std::get<ShortString>(other.m_data);
        }
        return asString() == other.asString();
    }
    if (type() != other.type()) return false;
    switch (type()) {
        case ValueType::NIL: return true;
        case ValueType::BOOL: return asBool() == other.asBool();
        case ValueType::ARRAY: {
            const auto& arr1 = asArray();
            const auto& arr2 = other.asArray();
            if (arr1.size() != arr2.size()) return false;
            for (size_t i = 0; i < arr1.size(); ++i) {
                if (!arr1[i].equals(arr2[i])) return false;
            }
            return true;
        }
        case ValueType::TUPLE: {
            const auto& tup1 = asTuple();
            const auto& tup2 = other.asTuple();
            if (tup1.size() != tup2.size()) return false;
            for (size_t i = 0; i < tup1.size(); ++i) {
                if (!tup1[i].equals(tup2[i])) return false;
            }
            return true;
        }
        case ValueType::DICTIONARY: {
            const auto& dict1 = asDictionary();
            const auto& dict2 = other.asDictionary();
            if (dict1.size() != dict2.size()) return false;
            
            bool isEqual = true;
            dict1.readMap([&](const std::unordered_map<std::string, Value>& m1) {
                dict2.readMap([&](const std::unordered_map<std::string, Value>& m2) {
                    for (const auto& [k, v1] : m1) {
                        auto it = m2.find(k);
                        if (it == m2.end() || !v1.equals(it->second)) {
                            isEqual = false;
                            break;
                        }
                    }
                });
            });
            return isEqual;
        }
        default: return m_data == other.m_data;
    }
}

inline Value Value::makeArray(const std::vector<Value>& elements) {
    auto ptr = std::make_shared<EZArray>(elements);
    CycleCollector::instance().track(ptr, ValueType::ARRAY);
    return Value(ptr);
}

inline Value Value::makeTuple(const std::vector<Value>& elements) {
    auto ptr = std::make_shared<EZTuple>(elements);
    CycleCollector::instance().track(ptr, ValueType::TUPLE);
    return Value(ptr);
}

inline Value Value::makeArrayCopy(const EZArray& other) {
    auto ptr = std::make_shared<EZArray>(other.getElementsCopy());
    CycleCollector::instance().track(ptr, ValueType::ARRAY);
    return Value(ptr);
}

inline Value Value::makeFunction(const std::string& name,
                          const std::vector<std::string>& params,
                          const std::vector<ExprPtr>& defaultValues,
                          const std::vector<StmtPtr>& body,
                          std::shared_ptr<Environment> closure,
                          bool variadic) {
    auto ptr = std::make_shared<EZFunction>(name, params, defaultValues, body, closure, variadic);
    CycleCollector::instance().track(ptr, ValueType::FUNCTION);
    return Value(ptr);
}

inline Value Value::makeNativeFunction(const std::string& name, int arity, NativeFn fn) {
    return Value(std::make_shared<NativeFunction>(name, arity, fn));
}

inline Value Value::makeDictionary() {
    auto ptr = std::make_shared<EZDictionary>();
    CycleCollector::instance().track(ptr, ValueType::DICTIONARY);
    return Value(ptr);
}
inline Value Value::makeSuper(InstancePtr instance, ClassPtr parentKlass) { 
    auto ptr = std::make_shared<EZSuper>(instance, parentKlass);
    CycleCollector::instance().track(ptr, ValueType::SUPER);
    return Value(ptr); 
}
inline Value Value::makeFuture(std::shared_ptr<EZFuture> fut) { return Value(fut); }
inline Value Value::makeClosure(ClosureValPtr closure) {
    CycleCollector::instance().track(closure, ValueType::CLOSURE_VAL);
    return Value(closure);
}
inline Value Value::makeAtomic(long long initial) { return Value(std::make_shared<EZAtomic>(initial)); }
inline Value Value::makeChannel(std::shared_ptr<EZChannel> chan) { return Value(chan); }




#endif // VALUEIMPL_H
