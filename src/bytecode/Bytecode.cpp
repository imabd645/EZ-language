#include "bytecode/Bytecode.h"
#include <iostream>
#include <iomanip>
#include <cstring>

// ============================================================================
// Chunk Implementation
// ============================================================================

size_t Chunk::addConstant(const Constant& constant) {
    if (constant.type == Constant::Type::INT || 
        constant.type == Constant::Type::DOUBLE ||
        constant.type == Constant::Type::STRING ||
        constant.type == Constant::Type::BOOL ||
        constant.type == Constant::Type::NIL) {
        
        for (size_t i = 0; i < constants.size(); ++i) {
            const auto& c = constants[i];
            if (c.type == constant.type) {
                if (c.type == Constant::Type::INT && std::get<long long>(c.value) == std::get<long long>(constant.value)) return i;
                if (c.type == Constant::Type::DOUBLE && std::get<double>(c.value) == std::get<double>(constant.value)) return i;
                if (c.type == Constant::Type::STRING && std::get<std::string>(c.value) == std::get<std::string>(constant.value)) return i;
                if (c.type == Constant::Type::BOOL && std::get<bool>(c.value) == std::get<bool>(constant.value)) return i;
                if (c.type == Constant::Type::NIL) return i;
            }
        }
    }
    constants.push_back(constant);
    return constants.size() - 1;
}

size_t Chunk::writeByte(uint8_t byte, size_t lineNum) {
    code.push_back(byte);
    lines.push_back(lineNum);
    return code.size() - 1;
}

void Chunk::writeOp(OpCode op, size_t lineNum) {
    writeByte(static_cast<uint8_t>(op), lineNum);
}

void Chunk::resolveConstants() {
    if (!resolvedConstants.empty()) return;
    resolvedConstants.reserve(constants.size());
    for (const auto& c : constants) {
        switch (c.type) {
            case Constant::Type::NIL:    resolvedConstants.push_back(Value()); break;
            case Constant::Type::BOOL:   resolvedConstants.push_back(Value(std::get<bool>(c.value))); break;
            case Constant::Type::INT:    resolvedConstants.push_back(Value(std::get<long long>(c.value))); break;
            case Constant::Type::DOUBLE: resolvedConstants.push_back(Value(std::get<double>(c.value))); break;
            case Constant::Type::STRING: resolvedConstants.push_back(Value(std::get<std::string>(c.value))); break;
            default: resolvedConstants.push_back(Value()); break;
        }
    }
}

void Chunk::writeBytes(uint8_t b1, uint8_t b2, size_t lineNum) {
    writeByte(b1, lineNum);
    writeByte(b2, lineNum);
}

void Chunk::writeBytes(uint8_t b1, uint8_t b2, uint8_t b3, size_t lineNum) {
    writeByte(b1, lineNum);
    writeByte(b2, lineNum);
    writeByte(b3, lineNum);
}

void Chunk::writeJump(OpCode op, size_t lineNum) {
    writeOp(op, lineNum);
    writeByte(0xFF, lineNum);
    writeByte(0xFF, lineNum);
    writeByte(0xFF, lineNum);
    writeByte(0xFF, lineNum);
}

void Chunk::patchJump(size_t offset) {
    // Calculate jump distance
    size_t jump = code.size() - offset - 4;
    
    if (jump > 0xFFFFFFFF) {
        std::cerr << "Error: Jump offset too large" << std::endl;
        return;
    }
    
    // Patch the four bytes (big-endian)
    code[offset] = (jump >> 24) & 0xFF;
    code[offset + 1] = (jump >> 16) & 0xFF;
    code[offset + 2] = (jump >> 8) & 0xFF;
    code[offset + 3] = jump & 0xFF;
}

void Chunk::writeLoop(size_t loopStart, size_t lineNum) {
    writeOp(OpCode::LOOP, lineNum);
    
    if (code.size() < loopStart) {
         std::cerr << "Error: Loop start after current code end!" << std::endl;
         return;
    }

    size_t offset = code.size() - loopStart + 4; // +4 bytes offset
    if (offset > 0xFFFFFFFF) {
        std::cerr << "Error: Loop body too large" << std::endl;
        return;
    }
    
    writeByte((offset >> 24) & 0xFF, lineNum);
    writeByte((offset >> 16) & 0xFF, lineNum);
    writeByte((offset >> 8) & 0xFF, lineNum);
    writeByte(offset & 0xFF, lineNum);
}

const Constant& Chunk::getConstant(size_t idx) const {
    static Constant nilConstant;
    if (idx >= constants.size()) {
        std::cerr << "Error: Constant index out of bounds: " << idx << std::endl;
        return nilConstant;
    }
    return constants[idx];
}

// ============================================================================
// Disassembly
// ============================================================================

static const char* opcodeName(OpCode op) {
    switch (op) {
        case OpCode::LOAD_CONST: return "LOAD_CONST";
        case OpCode::LOAD_LOCAL: return "LOAD_LOCAL";
        case OpCode::STORE_LOCAL: return "STORE_LOCAL";
        case OpCode::LOAD_UPVALUE: return "LOAD_UPVALUE";
        case OpCode::STORE_UPVALUE: return "STORE_UPVALUE";
        case OpCode::LOAD_GLOBAL: return "LOAD_GLOBAL";
        case OpCode::STORE_GLOBAL: return "STORE_GLOBAL";
        case OpCode::LOAD_PROPERTY: return "LOAD_PROPERTY";
        case OpCode::STORE_PROPERTY: return "STORE_PROPERTY";
        case OpCode::POP: return "POP";
        case OpCode::DUP: return "DUP";
        case OpCode::DUP2: return "DUP2";
        case OpCode::LOAD_NIL: return "LOAD_NIL";
        case OpCode::LOAD_TRUE: return "LOAD_TRUE";
        case OpCode::LOAD_FALSE: return "LOAD_FALSE";
        case OpCode::LOAD_ZERO: return "LOAD_ZERO";
        case OpCode::LOAD_ONE: return "LOAD_ONE";
        case OpCode::LOAD_EMPTY_STR: return "LOAD_EMPTY_STR";
        case OpCode::INC_LOCAL: return "INC_LOCAL";
        case OpCode::DEC_LOCAL: return "DEC_LOCAL";
        case OpCode::ADD: return "ADD";
        case OpCode::SUB: return "SUB";
        case OpCode::MUL: return "MUL";
        case OpCode::DIV: return "DIV";
        case OpCode::MOD: return "MOD";
        case OpCode::POW: return "POW";
        case OpCode::NEGATE: return "NEGATE";
        case OpCode::BIT_AND: return "BIT_AND";
        case OpCode::BIT_OR: return "BIT_OR";
        case OpCode::BIT_XOR: return "BIT_XOR";
        case OpCode::BIT_NOT: return "BIT_NOT";
        case OpCode::SHIFT_LEFT: return "SHIFT_LEFT";
        case OpCode::SHIFT_RIGHT: return "SHIFT_RIGHT";
        case OpCode::EQUAL: return "EQUAL";
        case OpCode::NOT_EQUAL: return "NOT_EQUAL";
        case OpCode::LESS: return "LESS";
        case OpCode::LESS_EQ: return "LESS_EQ";
        case OpCode::GREATER: return "GREATER";
        case OpCode::GREATER_EQ: return "GREATER_EQ";
        case OpCode::NOT: return "NOT";
        case OpCode::JUMP: return "JUMP";
        case OpCode::JUMP_IF_FALSE: return "JUMP_IF_FALSE";
        case OpCode::JUMP_IF_TRUE: return "JUMP_IF_TRUE";
        case OpCode::JUMP_IF_NIL: return "JUMP_IF_NIL";
        case OpCode::JUMP_IF_NOT_NIL: return "JUMP_IF_NOT_NIL";
        case OpCode::LOOP: return "LOOP";
        case OpCode::CALL: return "CALL";
        case OpCode::TAIL_CALL: return "TAIL_CALL";
        case OpCode::CALL_KW: return "CALL_KW";
        case OpCode::RETURN: return "RETURN";
        case OpCode::CLOSURE: return "CLOSURE";
        case OpCode::CLOSE_UPVALUE: return "CLOSE_UPVALUE";
        case OpCode::MAKE_ARRAY: return "MAKE_ARRAY";
        case OpCode::BUILD_TUPLE: return "BUILD_TUPLE";
        case OpCode::MAKE_DICT: return "MAKE_DICT";
        case OpCode::INDEX_GET: return "INDEX_GET";
        case OpCode::INDEX_SET: return "INDEX_SET";
        case OpCode::ARRAY_APPEND: return "ARRAY_APPEND";
        case OpCode::ARRAY_EXTEND: return "ARRAY_EXTEND";
        case OpCode::CALL_SPREAD: return "CALL_SPREAD";
        case OpCode::NEW_INSTANCE: return "NEW_INSTANCE";
        case OpCode::GET_METHOD: return "GET_METHOD";
        case OpCode::SUPER: return "SUPER";
        case OpCode::SUPER_CALL: return "SUPER_CALL";
        case OpCode::GET_ITER: return "GET_ITER";
        case OpCode::GET_DICT_ITER: return "GET_DICT_ITER";
        case OpCode::ITER_NEXT: return "ITER_NEXT";
        case OpCode::ITER_HAS_NEXT: return "ITER_HAS_NEXT";
        case OpCode::TRY_START: return "TRY_START";
        case OpCode::TRY_END: return "TRY_END";
        case OpCode::THROW: return "THROW";
        case OpCode::FINALLY_START: return "FINALLY_START";
        case OpCode::FINALLY_END: return "FINALLY_END";
        case OpCode::TO_STRING: return "TO_STRING";
        case OpCode::PRINT: return "PRINT";
        case OpCode::CLOCK: return "CLOCK";
        case OpCode::TYPE_OF: return "TYPE_OF";
        case OpCode::IS_INSTANCE_OF: return "IS_INSTANCE_OF";
        case OpCode::OP_AWAIT: return "AWAIT";
        case OpCode::MAKE_INTERFACE: return "MAKE_INTERFACE";
        case OpCode::MAKE_CLASS: return "MAKE_CLASS";
        case OpCode::BREAKPOINT: return "BREAKPOINT";
        case OpCode::LINE: return "LINE";
        case OpCode::HAS_GLOBAL: return "HAS_GLOBAL";
        case OpCode::LOAD_GLOBAL_SLOT: return "LOAD_GLOBAL_SLOT";
        case OpCode::STORE_GLOBAL_SLOT: return "STORE_GLOBAL_SLOT";
        case OpCode::LOOP_LESS_EQ_LOCAL: return "LOOP_LESS_EQ_LOCAL";
        case OpCode::LOOP_GREATER_EQ_LOCAL: return "LOOP_GREATER_EQ_LOCAL";
        case OpCode::INTERCEPTED_STORE_PROPERTY: return "INTERCEPTED_STORE_PROPERTY";
        case OpCode::RATELIMIT_CHECK: return "RATELIMIT_CHECK";
        case OpCode::GET_CACHED_RESULT: return "GET_CACHED_RESULT";
        case OpCode::STORE_CACHED_RESULT: return "STORE_CACHED_RESULT";
        case OpCode::LOAD_LOCAL_W: return "LOAD_LOCAL_W";
        case OpCode::STORE_LOCAL_W: return "STORE_LOCAL_W";
        case OpCode::MEMBER_IN: return "IN";
        case OpCode::ADD_LOCAL_LOCAL: return "ADD_LOCAL_LOCAL";
        case OpCode::SUB_LOCAL_LOCAL: return "SUB_LOCAL_LOCAL";
        case OpCode::INC_LOCAL_BY: return "INC_LOCAL_BY";
        case OpCode::ADD_GLOBAL_LOCAL: return "ADD_GLOBAL_LOCAL";
        case OpCode::PRINT_STR: return "PRINT_STR";
        case OpCode::INVOKE_METHOD: return "INVOKE_METHOD";
        case OpCode::END: return "END";
        default: return "UNKNOWN";
    }
}

static void printConstant(const Constant& constant) {
    switch (constant.type) {
        case Constant::Type::NIL:
            std::cout << "nil";
            break;
        case Constant::Type::BOOL:
            std::cout << (std::get<bool>(constant.value) ? "true" : "false");
            break;
        case Constant::Type::INT:
            std::cout << std::get<long long>(constant.value);
            break;
        case Constant::Type::DOUBLE:
            std::cout << std::get<double>(constant.value);
            break;
        case Constant::Type::STRING:
            std::cout << "\"" << std::get<std::string>(constant.value) << "\"";
            break;
        case Constant::Type::FUNCTION:
            std::cout << "<function>";
            break;
        case Constant::Type::MODEL:
            std::cout << "<model>";
            break;
        case Constant::Type::ARRAY_CONST:
            std::cout << "<array const>";
            break;
    }
}

void Chunk::disassemble(const std::string& name, const std::vector<std::string>* globalSlotNames, const std::vector<std::shared_ptr<struct BytecodeFunction>>* nestedFunctions) const {
    std::cout << "=== " << name << " ===" << std::endl;
    std::cout << "Constants: " << constants.size() << std::endl;
    
    // Print constant pool
    for (size_t i = 0; i < constants.size(); i++) {
        std::cout << std::setw(4) << i << " | ";
        printConstant(constants[i]);
        std::cout << std::endl;
    }
    
    std::cout << "--- Code ---" << std::endl;
    
    size_t offset = 0;
    while (offset < code.size()) {
        offset = disassembleInstruction(offset, globalSlotNames, nestedFunctions);
    }
}

size_t Chunk::disassembleInstruction(size_t offset, const std::vector<std::string>* globalSlotNames, const std::vector<std::shared_ptr<struct BytecodeFunction>>* nestedFunctions) const {
    std::cout << std::setw(4) << offset << " ";
    
    // Print line number
    if (offset > 0 && lines[offset] == lines[offset - 1]) {
        std::cout << "   | ";
    } else {
        std::cout << std::setw(4) << lines[offset] << " ";
    }
    
    uint8_t instruction = code[offset];
    OpCode op = static_cast<OpCode>(instruction);
    
    std::cout << std::setw(16) << opcodeName(op) << " ";
    
    switch (op) {
        case OpCode::LOAD_LOCAL_W:
        case OpCode::STORE_LOCAL_W: {
            uint16_t slot = (uint16_t)((code[offset + 1] << 8) | code[offset + 2]);
            std::cout << std::setw(4) << (int)slot << " ";
            std::cout << std::endl;
            return offset + 3;
        }
        case OpCode::LOAD_LOCAL:
        case OpCode::STORE_LOCAL:
        case OpCode::LOAD_UPVALUE:
        case OpCode::STORE_UPVALUE:
        case OpCode::MAKE_ARRAY:
        case OpCode::BUILD_TUPLE:
        case OpCode::MAKE_DICT:
        case OpCode::INC_LOCAL:
        case OpCode::DEC_LOCAL:
        case OpCode::CALL:
        case OpCode::TAIL_CALL:
        case OpCode::CALL_KW:
        case OpCode::CLOSE_UPVALUE: {
            uint8_t idx = code[offset + 1];
            std::cout << std::setw(4) << (int)idx << " ";
            std::cout << std::endl;
            return offset + 2;
        }

        case OpCode::NEW_INSTANCE: {
            uint16_t nameIdx = (uint16_t)((code[offset + 1] << 8) | code[offset + 2]);
            uint8_t argc = code[offset + 3];
            std::cout << std::setw(4) << (int)nameIdx;
            if (nameIdx < constants.size()) {
                std::cout << " (";
                printConstant(constants[nameIdx]);
                std::cout << ")";
            }
            std::cout << " argc=" << (int)argc << std::endl;
            return offset + 4;
        }

        case OpCode::SUPER_CALL: {
            uint16_t nameIdx = (uint16_t)((code[offset + 1] << 8) | code[offset + 2]);
            uint8_t argc = code[offset + 3];
            std::cout << std::setw(4) << (int)nameIdx;
            if (nameIdx < constants.size()) {
                std::cout << " (";
                printConstant(constants[nameIdx]);
                std::cout << ")";
            }
            std::cout << " argc=" << (int)argc << std::endl;
            return offset + 4;
        }

        case OpCode::MAKE_INTERFACE: {
            uint16_t nameIdx = (uint16_t)((code[offset + 1] << 8) | code[offset + 2]);
            uint8_t methodCount = code[offset + 3];
            std::cout << std::setw(4) << (int)nameIdx;
            if (nameIdx < constants.size()) {
                std::cout << " (";
                printConstant(constants[nameIdx]);
                std::cout << ")";
            }
            std::cout << " methods=" << (int)methodCount << std::endl;
            return offset + 4;
        }

        case OpCode::MAKE_CLASS: {
            uint16_t nameIdx = (uint16_t)((code[offset + 1] << 8) | code[offset + 2]);
            uint8_t memberCount = code[offset + 3];
            uint8_t ifCount = code[offset + 4];
            uint8_t valCount = code[offset + 5];
            std::cout << std::setw(4) << (int)nameIdx;
            if (nameIdx < constants.size()) {
                std::cout << " (";
                printConstant(constants[nameIdx]);
                std::cout << ")";
            }
            std::cout << " members=" << (int)memberCount
                      << " interfaces=" << (int)ifCount
                      << " validators=" << (int)valCount << std::endl;
            return offset + 6;
        }
        
        case OpCode::CLOSURE: {
            uint16_t idx = (uint16_t)((code[offset + 1] << 8) | code[offset + 2]);
            std::cout << std::setw(4) << (int)idx << std::endl;
            size_t newOffset = offset + 3;
            if (nestedFunctions && idx < nestedFunctions->size()) {
                size_t upvalueCount = (*nestedFunctions)[idx]->upvalues.size();
                for (size_t i = 0; i < upvalueCount; ++i) {
                    uint8_t isLocal = code[newOffset++];
                    uint16_t uvIndex = (uint16_t)((code[newOffset] << 8) | code[newOffset + 1]);
                    newOffset += 2;
                    std::cout << std::setw(4) << offset << "   |             " 
                              << (isLocal ? "upval local " : "upval upval ") 
                              << (int)uvIndex << std::endl;
                }
            }
            return newOffset;
        }
        
        case OpCode::LOAD_GLOBAL_SLOT:
        case OpCode::STORE_GLOBAL_SLOT: {
            uint16_t idx = (uint16_t)((code[offset + 1] << 8) | code[offset + 2]);
            std::cout << std::setw(4) << (int)idx;
            if (globalSlotNames && idx < globalSlotNames->size() && !(*globalSlotNames)[idx].empty()) {
                std::cout << " (\"" << (*globalSlotNames)[idx] << "\")";
            }
            std::cout << std::endl;
            return offset + 3;
        }

        case OpCode::LOAD_CONST:
        case OpCode::LOAD_GLOBAL:
        case OpCode::STORE_GLOBAL:
        case OpCode::LOAD_PROPERTY:
        case OpCode::STORE_PROPERTY:
        case OpCode::HAS_GLOBAL:
        case OpCode::GET_METHOD:
        case OpCode::INTERCEPTED_STORE_PROPERTY:
        case OpCode::RATELIMIT_CHECK:
        case OpCode::GET_CACHED_RESULT:
        case OpCode::STORE_CACHED_RESULT: {
            uint16_t idx = (uint16_t)((code[offset + 1] << 8) | code[offset + 2]);
            std::cout << std::setw(4) << (int)idx;
            if (idx < constants.size()) {
                std::cout << " (";
                printConstant(constants[idx]);
                std::cout << ")";
            }
            std::cout << std::endl;
            return offset + 3;  // opcode + 2 bytes
        }
        
        case OpCode::JUMP:
        case OpCode::JUMP_IF_FALSE:
        case OpCode::JUMP_IF_TRUE:
        case OpCode::JUMP_IF_NIL:
        case OpCode::JUMP_IF_NOT_NIL:
        case OpCode::TRY_START: {
            uint32_t jump = (code[offset + 1] << 24) | (code[offset + 2] << 16) | (code[offset + 3] << 8) | code[offset + 4];
            std::cout << std::setw(4) << (int)jump << " -> " << (offset + 5 + jump) << std::endl;
            return offset + 5;
        }
        
        case OpCode::LOOP: {
            uint32_t jump = (code[offset + 1] << 24) | (code[offset + 2] << 16) | (code[offset + 3] << 8) | code[offset + 4];
            std::cout << std::setw(4) << (int)jump << " -> " << (offset + 5 - jump) << std::endl;
            return offset + 5;
        }

        case OpCode::LOOP_LESS_EQ_LOCAL:
        case OpCode::LOOP_GREATER_EQ_LOCAL: {
            uint8_t  loopSlot = code[offset + 1];
            uint8_t  endSlot  = code[offset + 2];
            uint32_t exitOff  = (code[offset + 3] << 24) | (code[offset + 4] << 16)
                               | (code[offset + 5] << 8)  |  code[offset + 6];
            std::cout << " loop=" << (int)loopSlot
                      << " end=" << (int)endSlot
                      << " exit->" << (offset + 7 + exitOff)
                      << std::endl;
            return offset + 7;  // opcode(1) + loopSlot(1) + endSlot(1) + offset(4)
        }

        case OpCode::ITER_NEXT: {
            uint32_t jump = (code[offset + 1] << 24) | (code[offset + 2] << 16) | (code[offset + 3] << 8) | code[offset + 4];
            std::cout << std::setw(4) << (int)jump << " -> " << (offset + 5 + jump) << std::endl;
            return offset + 5;
        }
        
        case OpCode::ADD_LOCAL_LOCAL:
        case OpCode::SUB_LOCAL_LOCAL:
        case OpCode::INC_LOCAL_BY: {
            uint8_t dstSlot = code[offset + 1];
            uint8_t srcSlot = code[offset + 2];
            std::cout << " dst=" << (int)dstSlot << " src/imm=" << (int)srcSlot << std::endl;
            return offset + 3;
        }

        case OpCode::ADD_GLOBAL_LOCAL: {
            uint16_t globalSlot = (uint16_t)((code[offset + 1] << 8) | code[offset + 2]);
            uint8_t  localSlot  = code[offset + 3];
            std::cout << " gSlot=" << (int)globalSlot << " lSlot=" << (int)localSlot;
            if (globalSlotNames && globalSlot < globalSlotNames->size() && !(*globalSlotNames)[globalSlot].empty()) {
                std::cout << " (\"" << (*globalSlotNames)[globalSlot] << "\")";
            }
            std::cout << std::endl;
            return offset + 4;
        }

        case OpCode::INVOKE_METHOD: {
            uint16_t nameIdx = (uint16_t)((code[offset + 1] << 8) | code[offset + 2]);
            uint16_t icIdx   = (uint16_t)((code[offset + 3] << 8) | code[offset + 4]);
            uint8_t  args    = code[offset + 5];
            std::cout << std::setw(4) << (int)nameIdx;
            if (nameIdx < constants.size()) {
                std::cout << " (";
                printConstant(constants[nameIdx]);
                std::cout << ")";
            }
            std::cout << " ic=" << (int)icIdx << " args=" << (int)args << std::endl;
            return offset + 6;
        }

        case OpCode::LINE: {
            uint8_t lineNum = code[offset + 1];
            std::cout << std::setw(4) << (int)lineNum << std::endl;
            return offset + 2;
        }
        
        default:
            std::cout << std::endl;
            return offset + 1;
    }
}

// ============================================================================
// Constant Constructors
// ============================================================================

Constant::Constant(FunctionConstant* f) : type(Type::FUNCTION) {
    value = f;
}

Constant::Constant(ModelConstant* m) : type(Type::MODEL) {
    value = m;
}
