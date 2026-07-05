#include "bytecode/serializer/BytecodeSerializer.h"
#include <stdexcept>

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
    
    uint32_t slotCount = 0;
    in.read(reinterpret_cast<char*>(&slotCount), sizeof(slotCount));
    outGlobalSlotNames.clear();
    for (uint32_t i = 0; i < slotCount; ++i) {
        outGlobalSlotNames.push_back(readString(in));
    }
    
    return readFunction(in);
}

void BytecodeSerializer::writeString(const std::string& str, std::ostream& out) {
    uint32_t len = str.length();
    out.write(reinterpret_cast<const char*>(&len), sizeof(len));
    if (len > 0) {
        out.write(str.data(), len);
    }
}

std::string BytecodeSerializer::readString(std::istream& in) {
    uint32_t len = 0;
    in.read(reinterpret_cast<char*>(&len), sizeof(len));
    if (len == 0) return "";
    std::string str(len, '\0');
    in.read(&str[0], len);
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
    uint32_t codeSize = 0;
    in.read(reinterpret_cast<char*>(&codeSize), sizeof(codeSize));
    if (codeSize > 0) {
        chunk.code.resize(codeSize);
        chunk.lines.resize(codeSize);
        in.read(reinterpret_cast<char*>(chunk.code.data()), codeSize);
        in.read(reinterpret_cast<char*>(chunk.lines.data()), codeSize * sizeof(size_t));
    }
    
    uint32_t constSize = 0;
    in.read(reinterpret_cast<char*>(&constSize), sizeof(constSize));
    for (uint32_t i = 0; i < constSize; ++i) {
        uint8_t t;
        in.read(reinterpret_cast<char*>(&t), sizeof(t));
        Constant c;
        c.type = static_cast<Constant::Type>(t);
        switch (c.type) {
            case Constant::Type::NIL:
                c.value = nullptr;
                break;
            case Constant::Type::BOOL: {
                bool b;
                in.read(reinterpret_cast<char*>(&b), sizeof(b));
                c.value = b;
                break;
            }
            case Constant::Type::INT: {
                long long v;
                in.read(reinterpret_cast<char*>(&v), sizeof(v));
                c.value = v;
                break;
            }
            case Constant::Type::DOUBLE: {
                double v;
                in.read(reinterpret_cast<char*>(&v), sizeof(v));
                c.value = v;
                break;
            }
            case Constant::Type::STRING: {
                c.value = readString(in);
                break;
            }
            case Constant::Type::ARRAY_CONST: {
                uint32_t len = 0;
                in.read(reinterpret_cast<char*>(&len), sizeof(len));
                std::vector<size_t> arr(len);
                if (len > 0) in.read(reinterpret_cast<char*>(arr.data()), len * sizeof(size_t));
                c.value = arr;
                break;
            }
            default:
                c.value = nullptr;
                break;
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

std::shared_ptr<BytecodeFunction> BytecodeSerializer::readFunction(std::istream& in) {
    auto func = std::make_shared<BytecodeFunction>("", 0);
    func->name = readString(in);
    func->filename = readString(in);
    
    uint32_t arity = 0;
    in.read(reinterpret_cast<char*>(&arity), sizeof(arity));
    func->arity = arity;
    
    bool flags[3];
    in.read(reinterpret_cast<char*>(flags), sizeof(flags));
    func->isVariadic = flags[0];
    func->isAsync = flags[1];
    func->isMethod = flags[2];
    
    uint32_t upCount = 0;
    in.read(reinterpret_cast<char*>(&upCount), sizeof(upCount));
    func->upvalueCount = upCount;
    
    uint32_t localCount = 0;
    in.read(reinterpret_cast<char*>(&localCount), sizeof(localCount));
    func->localCount = localCount;
    
    uint32_t upSize = 0;
    in.read(reinterpret_cast<char*>(&upSize), sizeof(upSize));
    for (uint32_t i = 0; i < upSize; ++i) {
        uint8_t t;
        uint32_t idx;
        in.read(reinterpret_cast<char*>(&t), sizeof(t));
        in.read(reinterpret_cast<char*>(&idx), sizeof(idx));
        func->upvalues.push_back({static_cast<Upvalue::Type>(t), idx});
    }
    
    uint32_t locInfoSize = 0;
    in.read(reinterpret_cast<char*>(&locInfoSize), sizeof(locInfoSize));
    for (uint32_t i = 0; i < locInfoSize; ++i) {
        LocalVarInfo lv;
        lv.name = readString(in);
        uint32_t slot = 0, spc = 0, epc = 0;
        in.read(reinterpret_cast<char*>(&slot), sizeof(slot));
        in.read(reinterpret_cast<char*>(&spc), sizeof(spc));
        in.read(reinterpret_cast<char*>(&epc), sizeof(epc));
        lv.slot = slot; lv.startPC = spc; lv.endPC = epc;
        func->localVars.push_back(lv);
    }
    
    readChunk(func->chunk, in);
    func->chunk.resolveConstants();
    
    uint32_t nestCount = 0;
    in.read(reinterpret_cast<char*>(&nestCount), sizeof(nestCount));
    for (uint32_t i = 0; i < nestCount; ++i) {
        func->nestedFunctions.push_back(readFunction(in));
    }
    
    return func;
}
