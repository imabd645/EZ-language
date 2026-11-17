#include "bytecode/serializer/BytecodeSerializer.h"
#include <algorithm>
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

    [[noreturn]] void verificationError(const BytecodeFunction& func,
                                        size_t offset,
                                        const std::string& reason) {
        throw std::runtime_error("Corrupt bytecode in function '" + func.name +
                                 "' at offset " + std::to_string(offset) +
                                 ": " + reason);
    }

    void verifyFunction(const std::shared_ptr<BytecodeFunction>& func,
                        const std::vector<std::string>& globalSlotNames,
                        int depth = 0,
                        const BytecodeFunction* enclosing = nullptr) {
        if (!func) throw std::runtime_error("Corrupt bytecode: null function");
        if (depth > EZC_MAX_NEST_DEPTH)
            throw std::runtime_error("Corrupt bytecode: nested-function recursion too deep");

        const auto& code = func->chunk.code;
        const auto& constants = func->chunk.constants;
        if (code.empty()) verificationError(*func, 0, "empty instruction stream");
        if (func->chunk.lines.size() != code.size())
            verificationError(*func, 0, "line table size does not match code size");
        if (func->localCount > EZC_MAX_COUNT || func->arity > EZC_MAX_COUNT)
            verificationError(*func, 0, "implausible function frame size");
        if (func->upvalueCount != func->upvalues.size())
            verificationError(*func, 0, "upvalue count does not match upvalue table");
        if (func->defaultParamCount > func->arity)
            verificationError(*func, 0, "default parameter count exceeds function arity");
        if (func->arity > func->localCount)
            verificationError(*func, 0, "function arity exceeds local slot count");

        for (const auto& c : constants) {
            if (c.type == Constant::Type::ARRAY_CONST) {
                const auto& indexes = std::get<std::vector<size_t>>(c.value);
                for (size_t idx : indexes) {
                    if (idx >= constants.size())
                        verificationError(*func, 0, "array constant references an invalid constant index");
                }
            }
        }
        for (const auto& upvalue : func->upvalues) {
            if (upvalue.type != Upvalue::Type::LOCAL && upvalue.type != Upvalue::Type::UPVALUE)
                verificationError(*func, 0, "invalid upvalue kind");
            if (!enclosing ||
                (upvalue.type == Upvalue::Type::LOCAL && upvalue.index >= enclosing->localCount) ||
                (upvalue.type == Upvalue::Type::UPVALUE && upvalue.index >= enclosing->upvalues.size()))
                verificationError(*func, 0, "upvalue references an invalid enclosing slot");
        }
        for (const auto& local : func->localVars) {
            if (local.slot >= func->localCount)
                verificationError(*func, local.startPC, "local-variable metadata has an invalid slot");
            const bool liveUntilReturn = local.endPC == UINT32_MAX;
            if (local.startPC > code.size() ||
                (!liveUntilReturn && (local.startPC > local.endPC || local.endPC > code.size())))
                verificationError(*func, local.startPC, "local-variable metadata has an invalid code range");
        }

        auto requireConstant = [&](size_t idx, size_t at, bool requireString) {
            if (idx >= constants.size())
                verificationError(*func, at, "constant index is out of range");
            if (requireString && constants[idx].type != Constant::Type::STRING)
                verificationError(*func, at, "name operand does not reference a string constant");
        };

        std::vector<bool> boundaries(code.size() + 1, false);
        std::vector<std::pair<size_t, size_t>> jumpTargets;
        size_t requiredICEntries = 0;
        size_t pc = 0;
        while (pc < code.size()) {
            const size_t instructionOffset = pc;
            boundaries[pc] = true;
            uint8_t rawOp = code[pc++];
            if (rawOp > static_cast<uint8_t>(OpCode::END))
                verificationError(*func, instructionOffset, "unknown opcode " + std::to_string(rawOp));
            OpCode op = static_cast<OpCode>(rawOp);

            auto need = [&](size_t count) {
                if (count > code.size() - pc)
                    verificationError(*func, instructionOffset, "truncated opcode operands");
            };
            auto read8 = [&]() -> uint8_t {
                need(1);
                return code[pc++];
            };
            auto read16 = [&]() -> uint16_t {
                need(2);
                uint16_t value = static_cast<uint16_t>((code[pc] << 8) | code[pc + 1]);
                pc += 2;
                return value;
            };
            auto read32 = [&]() -> uint32_t {
                need(4);
                uint32_t value = (static_cast<uint32_t>(code[pc]) << 24) |
                                 (static_cast<uint32_t>(code[pc + 1]) << 16) |
                                 (static_cast<uint32_t>(code[pc + 2]) << 8) |
                                 static_cast<uint32_t>(code[pc + 3]);
                pc += 4;
                return value;
            };
            auto checkLocal = [&](size_t slot) {
                if (slot >= func->localCount)
                    verificationError(*func, instructionOffset, "local slot is out of range");
            };
            auto checkUpvalue = [&](size_t idx) {
                if (idx >= func->upvalues.size())
                    verificationError(*func, instructionOffset, "upvalue index is out of range");
            };
            auto checkGlobal = [&](size_t idx) {
                if (idx >= globalSlotNames.size())
                    verificationError(*func, instructionOffset, "global slot index is out of range");
            };
            auto checkIC = [&](size_t idx) {
                requiredICEntries = std::max(requiredICEntries, idx + 1);
            };
            auto readName = [&]() { requireConstant(read16(), instructionOffset, true); };
            auto readIC = [&]() { checkIC(read16()); };
            auto readForwardJump = [&]() {
                uint32_t distance = read32();
                if (distance > code.size() - pc)
                    verificationError(*func, instructionOffset, "forward jump leaves instruction stream");
                jumpTargets.push_back({instructionOffset, pc + distance});
            };
            auto readBackwardJump = [&]() {
                uint32_t distance = read32();
                if (distance > pc)
                    verificationError(*func, instructionOffset, "backward jump leaves instruction stream");
                jumpTargets.push_back({instructionOffset, pc - distance});
            };

            switch (op) {
                case OpCode::LOAD_CONST:
                    requireConstant(read16(), instructionOffset, false);
                    break;
                case OpCode::LOAD_LOCAL:
                case OpCode::STORE_LOCAL:
                case OpCode::INC_LOCAL:
                case OpCode::DEC_LOCAL:
                case OpCode::CLOSE_UPVALUE:
                    checkLocal(read8());
                    break;
                case OpCode::LOAD_LOCAL_W:
                case OpCode::STORE_LOCAL_W:
                    checkLocal(read16());
                    break;
                case OpCode::LOAD_UPVALUE:
                case OpCode::STORE_UPVALUE:
                    checkUpvalue(read8());
                    break;
                case OpCode::LOAD_GLOBAL:
                case OpCode::STORE_GLOBAL:
                case OpCode::HAS_GLOBAL:
                case OpCode::GET_METHOD:
                case OpCode::RATELIMIT_CHECK:
                case OpCode::GET_CACHED_RESULT:
                case OpCode::STORE_CACHED_RESULT:
                    readName();
                    break;
                case OpCode::LOAD_PROPERTY:
                case OpCode::STORE_PROPERTY:
                case OpCode::INTERCEPTED_STORE_PROPERTY:
                    readName();
                    readIC();
                    break;
                case OpCode::INVOKE_METHOD:
                    readName();
                    readIC();
                    read8(); // argument count
                    break;
                case OpCode::NEW_INSTANCE:
                case OpCode::SUPER_CALL:
                    readName();
                    read8(); // argument count
                    break;
                case OpCode::JUMP:
                case OpCode::JUMP_IF_FALSE:
                case OpCode::JUMP_IF_TRUE:
                case OpCode::JUMP_IF_NIL:
                case OpCode::JUMP_IF_NOT_NIL:
                case OpCode::ITER_NEXT:
                case OpCode::TRY_START:
                    readForwardJump();
                    break;
                case OpCode::LOOP:
                    readBackwardJump();
                    break;
                case OpCode::LOOP_LESS_EQ_LOCAL:
                case OpCode::LOOP_GREATER_EQ_LOCAL: {
                    checkLocal(read8());
                    checkLocal(read8());
                    readForwardJump();
                    break;
                }
                case OpCode::CALL:
                case OpCode::TAIL_CALL:
                case OpCode::CALL_KW:
                case OpCode::MAKE_ARRAY:
                case OpCode::BUILD_TUPLE:
                case OpCode::MAKE_DICT:
                    read8(); // count
                    break;
                case OpCode::CLOSURE: {
                    uint16_t nestedIndex = read16();
                    if (nestedIndex >= func->nestedFunctions.size() || !func->nestedFunctions[nestedIndex])
                        verificationError(*func, instructionOffset, "closure references an invalid nested function");
                    const auto& nested = func->nestedFunctions[nestedIndex];
                    for (size_t i = 0; i < nested->upvalues.size(); ++i) {
                        uint8_t isLocal = read8();
                        uint16_t idx = read16();
                        if (isLocal > 1)
                            verificationError(*func, instructionOffset, "invalid closure capture descriptor");
                        const auto& expected = nested->upvalues[i];
                        bool expectedLocal = expected.type == Upvalue::Type::LOCAL;
                        if ((isLocal != 0) != expectedLocal || idx != expected.index)
                            verificationError(*func, instructionOffset, "closure capture disagrees with nested function metadata");
                        if (isLocal) checkLocal(idx);
                        else checkUpvalue(idx);
                    }
                    break;
                }
                case OpCode::MAKE_INTERFACE:
                    readName();
                    read8(); // method count
                    break;
                case OpCode::MAKE_CLASS:
                    readName();
                    read8(); // member count
                    read8(); // interface count
                    read8(); // validator count
                    break;
                case OpCode::LOAD_GLOBAL_SLOT:
                case OpCode::STORE_GLOBAL_SLOT:
                    checkGlobal(read16());
                    break;
                case OpCode::LINE:
                    read16();
                    break;
                case OpCode::ADD_LOCAL_LOCAL:
                case OpCode::SUB_LOCAL_LOCAL:
                    checkLocal(read8());
                    checkLocal(read8());
                    break;
                case OpCode::INC_LOCAL_BY:
                    checkLocal(read8());
                    read8(); // signed immediate
                    break;
                case OpCode::ADD_GLOBAL_LOCAL:
                    checkGlobal(read16());
                    checkLocal(read8());
                    break;
                default:
                    // All remaining opcodes have no encoded operands.
                    break;
            }
        }
        boundaries[code.size()] = true;
        for (const auto& target : jumpTargets) {
            if (target.second >= code.size() || !boundaries[target.second])
                verificationError(*func, target.first, "jump target is not an instruction boundary");
        }
        uint32_t previousCallSiteOffset = 0;
        bool firstCallSite = true;
        for (const auto& callSite : func->callSites) {
            if (callSite.offset >= code.size() || !boundaries[callSite.offset])
                verificationError(*func, callSite.offset, "call-site metadata is not on an instruction boundary");
            if (!firstCallSite && callSite.offset < previousCallSiteOffset)
                verificationError(*func, callSite.offset, "call-site metadata is not ordered");
            previousCallSiteOffset = callSite.offset;
            firstCallSite = false;
        }

        // Inline caches are runtime-only and intentionally absent from .ezc.
        // Recreate empty entries to the highest verified index before execution.
        func->chunk.icEntries.resize(requiredICEntries);

        for (const auto& nested : func->nestedFunctions)
            verifyFunction(nested, globalSlotNames, depth + 1, func.get());
    }
}

void BytecodeSerializer::serialize(const std::shared_ptr<BytecodeFunction>& func, const std::vector<std::string>& globalSlotNames, std::ostream& out) {
    // Magic header
    // EZC2: the CLOSURE upvalue descriptor grew from 2 bytes to 3 (the capture
    // index is now 16-bit), so an EZC1 file cannot be decoded by this VM.
    // EZC3: each function now carries its call-site name table, so a bundled
    // program reports "'save' is nil" like a script does instead of falling
    // back to the bare type.
    out.write("EZC3", 4);
    
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
    if (in.gcount() != 4) {
        throw std::runtime_error("Invalid EZC file format: the file is too short to be bytecode");
    }
    std::string tag(magic, 4);
    // Naming the version the file actually carries turns "invalid format" into
    // an instruction: an old bundle needs recompiling, anything else is not a
    // .ezc at all.
    if (tag == "EZC1" || tag == "EZC2") {
        throw std::runtime_error(
            "This .ezc was produced by an older interpreter (format " + tag +
            "; this one reads EZC3) and cannot be run by it. "
            "Recompile it from the .ez source.");
    }
    if (tag != "EZC3") {
        throw std::runtime_error("Invalid EZC file format: expected an EZC3 header");
    }
    
    uint32_t slotCount = readCount(in, "global slot count");
    outGlobalSlotNames.clear();
    for (uint32_t i = 0; i < slotCount; ++i) {
        outGlobalSlotNames.push_back(readString(in));
    }

    auto function = readFunction(in, 0);
    if (in.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("Corrupt bytecode: trailing data after main function");
    verifyFunction(function, outGlobalSlotNames);
    return function;
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
    
    // Parameter names. Not needed to execute, but the arity error prints the
    // signature, and without these a bundled program says "needs 2 arguments"
    // where a script says "needs 2 arguments -- Signature: connect(host, port)".
    uint32_t paramCount = func->paramNames.size();
    out.write(reinterpret_cast<const char*>(&paramCount), sizeof(paramCount));
    for (const auto& param : func->paramNames) writeString(param, out);

    // Call-site names, for error messages. Written after the chunk because it
    // indexes into the chunk's code offsets.
    uint32_t callSiteCount = func->callSites.size();
    out.write(reinterpret_cast<const char*>(&callSiteCount), sizeof(callSiteCount));
    for (const auto& cs : func->callSites) {
        out.write(reinterpret_cast<const char*>(&cs.offset), sizeof(cs.offset));
        writeString(cs.name, out);
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

    uint32_t paramCount = readCount(in, "parameter-name count");
    func->paramNames.reserve(paramCount);
    for (uint32_t i = 0; i < paramCount; ++i) func->paramNames.push_back(readString(in));

    uint32_t callSiteCount = readCount(in, "call-site table size");
    func->callSites.reserve(callSiteCount);
    for (uint32_t i = 0; i < callSiteCount; ++i) {
        CallSiteInfo cs;
        cs.offset = readPod<uint32_t>(in, "call-site offset");
        cs.name   = readString(in);
        func->callSites.push_back(std::move(cs));
    }

    readChunk(func->chunk, in);
    func->chunk.resolveConstants();

    uint32_t nestCount = readCount(in, "nested-function count");
    for (uint32_t i = 0; i < nestCount; ++i) {
        func->nestedFunctions.push_back(readFunction(in, depth + 1));
    }

    return func;
}
