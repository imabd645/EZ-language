#include "runtime/objects/EZObjects.h"
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "vm/BytecodeVM.h"
#include "runtime/Value.h"
#include <iostream>

void BytecodeVM::closeUpvalues(Value* last) {
    while (openUpvalues != nullptr && openUpvalues->location.load() >= last) {
        std::shared_ptr<UpvalueObj> uv = openUpvalues;
        uv->closed = *uv->location.load();
        uv->location.store(&uv->closed);
        openUpvalues = uv->next;
        uv->next = nullptr; // unlink so a removed upvalue doesn't retain the rest of the list
    }
}

// ============================================================================
// Binary Operations
// ============================================================================

void BytecodeVM::defineGlobal(const std::string& name, const Value& value) {
    globalEnv->define(name, value);
}

Value BytecodeVM::eval(const std::string& code, const std::string& filename) {
    // Since globalSlots are now shared in globalEnv, we don't need to manually sync them back to the hash map.

    Lexer lexer(code, filename);
    std::vector<Token> tokens = lexer.tokenize();
    if (lexer.hasError()) {
        throwException("SyntaxError", "Syntax error in eval()", 0, filename);
        return Value();
    }

    ASTArena arena;
    Parser parser(tokens, arena);
    std::vector<StmtPtr> statements = parser.parse();
    if (parser.hasError()) {
        throwException("SyntaxError", "Parse error in eval()", 0, filename);
        return Value();
    }

    BytecodeCompiler compiler(arena);
    
    // Seed the compiler with existing globals to avoid slot collisions
    {
        std::shared_lock<std::shared_mutex> lock(globalEnv->slotMutex);
        for (size_t i = 0; i < globalEnv->globalSlotNames.size(); ++i) {
            if (!globalEnv->globalSlotNames[i].empty()) {
                compiler.setGlobalSlot(globalEnv->globalSlotNames[i], i);
            }
        }
    }
    
    CompileResult result = compiler.compile(statements);
    if (!result.success) return Value();

    // Merge any new slots the eval'd code introduced
    if (!result.globalSlotNames.empty()) {
        std::unique_lock<std::shared_mutex> lock(globalEnv->slotMutex);
        size_t newCount = result.globalSlotNames.size();
        if (newCount > globalEnv->globalSlots.size()) {
            globalEnv->globalSlots.resize(newCount, Value());
            globalEnv->globalSlotNames.resize(newCount);
        }
        for (size_t i = 0; i < newCount; ++i) {
            if (globalEnv->globalSlotNames[i].empty() && !result.globalSlotNames[i].empty()) {
                globalEnv->globalSlotNames[i] = result.globalSlotNames[i];
                // Seed from globalEnv if already defined
                if (globalEnv->contains(globalEnv->globalSlotNames[i]))
                    globalEnv->globalSlots[i] = globalEnv->get(globalEnv->globalSlotNames[i]);
            }
        }
    }

    return execute(result.mainFunction);
}

std::string BytecodeVM::stringify(const Value& val, int line, const std::string& filename) {
    return val.toString();
}

// ============================================================================
// Built-ins
// ============================================================================

