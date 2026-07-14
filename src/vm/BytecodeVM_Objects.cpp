#include "runtime/objects/EZObjects.h"
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "vm/BytecodeVM.h"
#include "runtime/Value.h"
#include <iostream>

void BytecodeVM::closeUpvalues(Value* last) {
    while (openUpvalues != nullptr && openUpvalues->location >= last) {
        UpvalueObj* uv = openUpvalues;
        uv->closed     = *uv->location.load();
        uv->location.store(&uv->closed);
        openUpvalues   = uv->next;
    }
}

// ============================================================================
// Binary Operations
// ============================================================================

void BytecodeVM::defineGlobal(const std::string& name, const Value& value) {
    globalEnv->define(name, value);
}

Value BytecodeVM::eval(const std::string& code, const std::string& filename) {
    // Sync all slot values back into globalEnv so the new code can see
    // globals that were written via STORE_GLOBAL_SLOT since the last sync.
    for (size_t i = 0; i < globalSlots.size() && i < globalSlotNames.size(); ++i) {
        if (!globalSlotNames[i].empty()) {
            globalEnv->assign(globalSlotNames[i], globalSlots[i]);
        }
    }

    Lexer lexer(code, filename);
    std::vector<Token> tokens = lexer.tokenize();
    if (lexer.hasError()) return Value();

    ASTArena arena;
    Parser parser(tokens, arena);
    std::vector<StmtPtr> statements = parser.parse();
    if (parser.hasError()) return Value();

    BytecodeCompiler compiler(arena);
    CompileResult result = compiler.compile(statements);
    if (!result.success) return Value();

    // Merge any new slots the eval'd code introduced
    if (!result.globalSlotNames.empty()) {
        size_t newCount = result.globalSlotNames.size();
        if (newCount > globalSlots.size()) {
            globalSlots.resize(newCount, Value());
            globalSlotNames.resize(newCount);
        }
        for (size_t i = 0; i < newCount; ++i) {
            if (globalSlotNames[i].empty() && !result.globalSlotNames[i].empty()) {
                globalSlotNames[i] = result.globalSlotNames[i];
                // Seed from globalEnv if already defined
                if (globalEnv->contains(globalSlotNames[i]))
                    globalSlots[i] = globalEnv->get(globalSlotNames[i]);
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

