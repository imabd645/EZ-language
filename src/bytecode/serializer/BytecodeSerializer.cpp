#include "bytecode/serializer/BytecodeSerializer.h"
#include <stdexcept>
#include <string>

// ── Deserialization hardening ────────────────────────────────────────────────
// A bundled .ezc is untrusted input. Reading it must never trust attacker-
// controlled lengths (huge-allocation DoS, len*sizeof overflow) or silently
// proceed past a truncated/corrupt stream (executing zero-filled garbage).
namespace {
    // Upper bound on any single serialized length/count. 64M comfortably exceeds
    // any real program's code/constant/function counts while capping allocation.
    constexpr uint32_t EZC_MAX_COUNT = 64u * 1024u * 1024u;
    // Cap on nested-function recursion depth (guards the readFunction recursion
    // against a maliciously deep nesting chain overflowing the C++ stack).
    constexpr int EZC_MAX_NEST_DEPTH = 512;

    template <typename T>
    T readPod(std::istream& in, const char* what) {
        T v{};
        in.read(reinterpret_cast<char*>(&v), sizeof(T));
        if (in.gcount() != static_cast<std::streamsize>(sizeof(T)))
            throw std::runtime_error(std::string("Corrupt bytecode: truncated ") + what);
        return v;
    }

    // Read a length/count and reject implausible values before any allocation.
    uint32_t readCount(std::istream& in, const char* what) {
        uint32_t n = readPod<uint32_t>(in, what);
        if (n > EZC_MAX_COUNT)
            throw std::runtime_error(std::string("Corrupt bytecode: implausible ") + what);
        return n;
    }

    // Read exactly `bytes` into dst, throwing on truncation.
    void readExact(std::istream& in, void* dst, size_t bytes, const char* what) {
        if (bytes == 0) return;
        in.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(bytes));
        if (in.gcount() != static_cast<std::streamsize>(bytes))
            throw std::runtime_error(std::string("Corrupt bytecode: truncated ") + what);
    }
}

void BytecodeSerializer::serialize(const std::shared_ptr<BytecodeFunction>& func, const std::vector<std::string>& globalSlotNames, std::ostream& out) {
    // Magic header
    out.write("EZC1", 4);
    
    // Global slot names
    uint32_t slotCount = globalSlotNames.size();
    out.write(reinterpret_cast<const char*>(&slotCount), sizeof(slotCount));
    for (const auto& name : globalSlotNames) {
        writeString(name, out);
    }
    
    writeFunction(func, out);
}

std::shared_ptr<BytecodeFunction> BytecodeSerializer::deserialize(std::istream& in, std::vector<std::string>& outGlobalSlotNames) {
    char magic[4];
    in.read(magic, 4);
    if (in.gcount() != 4 || std::string(magic, 4) != "EZC1") {
        throw std::runtime_error("Invalid EZC file format");
    }
    
    uint32_t slotCount = readCount(in, "global slot count");
    outGlobalSlotNames.clear();
    for (uint32_t i = 0; i < slotCount; ++i) {
        outGlobalSlotNames.push_back(readString(in));
    }

    return readFunction(in, 0);
}

void BytecodeSerializer::writeString(const std::string& str, std::ostream& out) {
    uint32_t len = str.length();
    out.write(reinterpret_cast<const char*>(&len), sizeof(len));
    if (len > 0) {
        out.write(str.data(), len);
    }
}

std::string BytecodeSerializer::readString(std::istream& in) {
    uint32_t len = readCount(in, "string length");
    if (len == 0) return "";
    std::string str(len, '\0');
    readExact(in, &str[0], len, "string data");
    return str;
}

void BytecodeSerializer::writeChunk(const Chunk& chunk, std::ostream& out) {
    uint32_t codeSize = chunk.code.size();
    out.write(reinterpret_cast<const char*>(&codeSize), sizeof(codeSize));
    if (codeSize > 0) {
        out.write(reinterpret_cast<const char*>(chunk.code.data()), codeSize);
        out.write(reinterpret_cast<const char*>(chunk.lines.data()), codeSize * sizeof(size_t));
    }
    
    uint32_t constSize = chunk.constants.size();
    out.write(reinterpret_cast<const char*>(&constSize), sizeof(constSize));
    for (const auto& c : chunk.constants) {
        uint8_t t = static_cast<uint8_t>(c.type);
        out.write(reinterpret_cast<const char*>(&t), sizeof(t));
        
        switch (c.type) {
            case Constant::Type::NIL:
                break;
            case Constant::Type::BOOL: {
                bool b = std::get<bool>(c.value);
                out.write(reinterpret_cast<const char*>(&b), sizeof(b));
                break;
            }
            case Constant::Type::INT: {
                long long i = std::get<long long>(c.value);
                out.write(reinterpret_cast<const char*>(&i), sizeof(i));
                break;
            }
            case Constant::Type::DOUBLE: {
                double d = std::get<double>(c.value);
                out.write(reinterpret_cast<const char*>(&d), sizeof(d));
                break;
            }
            case Constant::Type::STRING: {
                writeString(std::get<std::string>(c.value), out);
                break;
            }
            case Constant::Type::ARRAY_CONST: {
                const auto& arr = std::get<std::vector<size_t>>(c.value);
                uint32_t len = arr.size();
                out.write(reinterpret_cast<const char*>(&len), sizeof(len));
                if (len > 0) out.write(reinterpret_cast<const char*>(arr.data()), len * sizeof(size_t));
                break;
            }
            default:
                break; // Unsupported constant type (Function/Model unused in EZ bytecode)
        }
    }
}

void BytecodeSerializer::readChunk(Chunk& chunk, std::istream& in) {
    uint32_t codeSize = readCount(in, "code size");
    if (codeSize > 0) {
        chunk.code.resize(codeSize);
        chunk.lines.resize(codeSize);
        readExact(in, chunk.code.data(), codeSize, "code");
        // codeSize is bounded by EZC_MAX_COUNT, so codeSize*sizeof(size_t) cannot
        // overflow size_t; readExact validates the stream actually holds it.
        readExact(in, chunk.lines.data(), static_cast<size_t>(codeSize) * sizeof(size_t), "line table");
    }

    uint32_t constSize = readCount(in, "constant count");
    for (uint32_t i = 0; i < constSize; ++i) {
        uint8_t t = readPod<uint8_t>(in, "constant tag");
        Constant c;
        c.type = static_cast<Constant::Type>(t);
        switch (c.type) {
            case Constant::Type::NIL:
            case Constant::Type::FUNCTION:   // payloadless in EZ bytecode
            case Constant::Type::MODEL:
                c.value = nullptr;
                break;
            case Constant::Type::BOOL:
                c.value = readPod<bool>(in, "bool constant");
                break;
            case Constant::Type::INT:
                c.value = readPod<long long>(in, "int constant");
                break;
            case Constant::Type::DOUBLE:
                c.value = readPod<double>(in, "double constant");
                break;
            case Constant::Type::STRING:
                c.value = readString(in);
                break;
            case Constant::Type::ARRAY_CONST: {
                uint32_t len = readCount(in, "array constant length");
                std::vector<size_t> arr(len);
                readExact(in, arr.data(), static_cast<size_t>(len) * sizeof(size_t), "array constant");
                c.value = arr;
                break;
            }
            default:
                // Tag outside the known enum range -> the stream is corrupt.
                throw std::runtime_error("Corrupt bytecode: unknown constant type tag");
        }
        chunk.addConstant(c);
    }
}

void BytecodeSerializer::writeFunction(const std::shared_ptr<BytecodeFunction>& func, std::ostream& out) {
    writeString(func->name, out);
    writeString(func->filename, out);
    
    uint32_t arity = func->arity;
    out.write(reinterpret_cast<const char*>(&arity), sizeof(arity));
    
    bool flags[3] = {func->isVariadic, func->isAsync, func->isMethod};
    out.write(reinterpret_cast<const char*>(flags), sizeof(flags));
    
    uint32_t upCount = func->upvalueCount;
    out.write(reinterpret_cast<const char*>(&upCount), sizeof(upCount));
    
    uint32_t localCount = func->localCount;
    out.write(reinterpret_cast<const char*>(&localCount), sizeof(localCount));
    
    uint32_t upSize = func->upvalues.size();
    out.write(reinterpret_cast<const char*>(&upSize), sizeof(upSize));
    for (const auto& uv : func->upvalues) {
        uint8_t t = static_cast<uint8_t>(uv.type);
        uint32_t idx = uv.index;
        out.write(reinterpret_cast<const char*>(&t), sizeof(t));
        out.write(reinterpret_cast<const char*>(&idx), sizeof(idx));
    }
    
    uint32_t locInfoSize = func->localVars.size();
    out.write(reinterpret_cast<const char*>(&locInfoSize), sizeof(locInfoSize));
    for (const auto& lv : func->localVars) {
        writeString(lv.name, out);
        uint32_t slot = lv.slot, spc = lv.startPC, epc = lv.endPC;
        out.write(reinterpret_cast<const char*>(&slot), sizeof(slot));
        out.write(reinterpret_cast<const char*>(&spc), sizeof(spc));
        out.write(reinterpret_cast<const char*>(&epc), sizeof(epc));
    }
    
    writeChunk(func->chunk, out);
    
    uint32_t nestCount = func->nestedFunctions.size();
    out.write(reinterpret_cast<const char*>(&nestCount), sizeof(nestCount));
    for (const auto& nf : func->nestedFunctions) {
        writeFunction(nf, out);
    }
}

std::shared_ptr<BytecodeFunction> BytecodeSerializer::readFunction(std::istream& in, int depth) {
    if (depth > EZC_MAX_NEST_DEPTH)
        throw std::runtime_error("Corrupt bytecode: nested-function recursion too deep");

    auto func = std::make_shared<BytecodeFunction>("", 0);
    func->name = readString(in);
    func->filename = readString(in);

    func->arity = readPod<uint32_t>(in, "arity");

    bool flags[3];
    readExact(in, flags, sizeof(flags), "function flags");
    func->isVariadic = flags[0];
    func->isAsync = flags[1];
    func->isMethod = flags[2];

    func->upvalueCount = readPod<uint32_t>(in, "upvalue count");
    func->localCount   = readPod<uint32_t>(in, "local count");

    uint32_t upSize = readCount(in, "upvalue table size");
    for (uint32_t i = 0; i < upSize; ++i) {
        uint8_t  t   = readPod<uint8_t>(in, "upvalue kind");
        uint32_t idx = readPod<uint32_t>(in, "upvalue index");
        func->upvalues.push_back({static_cast<Upvalue::Type>(t), idx});
    }

    uint32_t locInfoSize = readCount(in, "local-var table size");
    for (uint32_t i = 0; i < locInfoSize; ++i) {
        LocalVarInfo lv;
        lv.name = readString(in);
        lv.slot    = readPod<uint32_t>(in, "local slot");
        lv.startPC = readPod<uint32_t>(in, "local startPC");
        lv.endPC   = readPod<uint32_t>(in, "local endPC");
        func->localVars.push_back(lv);
    }

    readChunk(func->chunk, in);
    func->chunk.resolveConstants();

    uint32_t nestCount = readCount(in, "nested-function count");
    for (uint32_t i = 0; i < nestCount; ++i) {
        func->nestedFunctions.push_back(readFunction(in, depth + 1));
    }

    return func;
}
