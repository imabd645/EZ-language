#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <memory>
namespace fs = std::filesystem;
#include "BytecodeCompiler.h"
#include "utils/EzLibPath.h"
#include "utils/WrapArith.h"
#include "lexer/Lexer.h"
#include "parser/Parser.h"

#include <iostream>
void BytecodeCompiler::compileStmt(const StmtPtr& stmt) {
    if (!stmt) return;

    currentLine = stmt->line;
    currentFile = stmt->filename;

    std::visit([this](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, ExpressionStmt*>) {
            compileExpressionStmt(*arg);
        } else if constexpr (std::is_same_v<T, OutStmt*>) {
            compileOutStmt(*arg);
        } else if constexpr (std::is_same_v<T, VarDeclStmt*>) {
            compileVarDecl(*arg);
        } else if constexpr (std::is_same_v<T, BlockStmt*>) {
            compileBlock(*arg);
        } else if constexpr (std::is_same_v<T, WhenStmt*>) {
            compileWhen(*arg);
        } else if constexpr (std::is_same_v<T, WhileStmt*>) {
            compileWhile(*arg);
        } else if constexpr (std::is_same_v<T, RepeatStmt*>) {
            compileRepeat(*arg);
        } else if constexpr (std::is_same_v<T, GetStmt*>) {
            compileGet(*arg);
        } else if constexpr (std::is_same_v<T, MatchStmt*>) {
            compileMatch(*arg);
        } else if constexpr (std::is_same_v<T, TaskStmt*>) {
            compileTask(*arg);
        } else if constexpr (std::is_same_v<T, GiveStmt*>) {
            compileGive(*arg);
        } else if constexpr (std::is_same_v<T, UseStmt*>) {
            compileUse(*arg);
        } else if constexpr (std::is_same_v<T, ExportStmt*>) {
            compileExport(*arg);
        } else if constexpr (std::is_same_v<T, EscapeStmt*>) {
            compileEscape(*arg);
        } else if constexpr (std::is_same_v<T, SkipStmt*>) {
            compileSkip(*arg);
        } else if constexpr (std::is_same_v<T, ModelStmt*>) {
            compileModel(*arg);
        } else if constexpr (std::is_same_v<T, StaticStmt*>) {
            compileStatic(*arg);
        } else if constexpr (std::is_same_v<T, TryStmt*>) {
            compileTry(*arg);
        } else if constexpr (std::is_same_v<T, ThrowStmt*>) {
            compileThrow(*arg);
        } else if constexpr (std::is_same_v<T, InterfaceStmt*>) {
            compileInterface(*arg);
        }
    }, stmt->variant);
}

void BytecodeCompiler::compileExpressionStmt(const ExpressionStmt& stmt) {
    compileExpr(stmt.expr);
    emitOp(OpCode::POP);
}

void BytecodeCompiler::compileOutStmt(const OutStmt& stmt) {
    compileExpr(stmt.expr);
    emitOp(OpCode::TO_STRING);
    emitOp(OpCode::PRINT);
}

void BytecodeCompiler::compileVarDecl(const VarDeclStmt& stmt) {
    if (stmt.initializer) {
        compileExpr(stmt.initializer);
    } else {
        emitOp(OpCode::LOAD_NIL);
    }

    if (current->scopeDepth == 0) {
        uint16_t slot = globalSlotFor(stmt.name);
        emitOp(OpCode::STORE_GLOBAL_SLOT);
        emitBytes(static_cast<uint8_t>((slot >> 8) & 0xFF),
                  static_cast<uint8_t>(slot & 0xFF));
        emitOp(OpCode::POP);
    } else {
        size_t localIdx = addLocal(stmt.name);
        markInitialized();
        emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL),
                  static_cast<uint8_t>(localIdx));
        // Local variables stay on stack (no POP)
    }
}

void BytecodeCompiler::compileBlock(const BlockStmt& stmt) {
    beginScope();
    for (const auto& s : stmt.statements) {
        compileStmt(s);
    }
    endScope();
}

void BytecodeCompiler::compileWhen(const WhenStmt& stmt) {
    compileExpr(stmt.condition);
    size_t elseJump = emitJump(OpCode::JUMP_IF_FALSE);

    compileStmt(stmt.thenBranch);

    if (stmt.elseBranch) {
        size_t endJump = emitJump(OpCode::JUMP);
        patchJump(elseJump);
        compileStmt(stmt.elseBranch);
        patchJump(endJump);
    } else {
        patchJump(elseJump);
    }
}

void BytecodeCompiler::compileWhile(const WhileStmt& stmt) {
    startLoop();
    size_t loopStart = currentChunk().code.size();
    loopStack.back().start = loopStart;

    compileExpr(stmt.condition);
    size_t exitJump = emitJump(OpCode::JUMP_IF_FALSE);

    compileStmt(stmt.body);
    emitLoop(loopStart);

    patchJump(exitJump);

    // Retrieve loop context again as the stack may have reallocated
    for (size_t breakOffset : loopStack.back().breaks) patchJump(breakOffset);
    endLoop();
}

void BytecodeCompiler::compileRepeat(const RepeatStmt& stmt) {
    beginScope();

    // Loop variable (slot N)
    size_t loopVar = addLocal(stmt.variable);
    compileExpr(stmt.start);
    markInitialized();
    emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL),
              static_cast<uint8_t>(loopVar));

    // End value cached in a hidden local (slot N+1)
    size_t endVar = addLocal("<repeat-end>");
    compileExpr(stmt.end);
    markInitialized();
    emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL),
              static_cast<uint8_t>(endVar));

    if (!stmt.step) {
        startLoop();
        size_t loopStart = currentChunk().code.size();
        loopStack.back().start = loopStart;

        emitOp(OpCode::LOOP_LESS_EQ_LOCAL);
        emitByte(static_cast<uint8_t>(loopVar));
        emitByte(static_cast<uint8_t>(endVar));
        emitByte(0xFF); emitByte(0xFF); emitByte(0xFF); emitByte(0xFF);
        size_t exitJump = currentChunk().code.size() - 4;

        compileStmt(stmt.body);

        emitBytes(static_cast<uint8_t>(OpCode::INC_LOCAL), static_cast<uint8_t>(loopVar));

        emitLoop(loopStart);
        patchJump(exitJump);
        for (size_t breakOffset : loopStack.back().breaks) patchJump(breakOffset);
        endLoop();
    } else {
        // Step value cached in a hidden local (slot N+2)
        size_t stepVar = addLocal("<repeat-step>");
        compileExpr(stmt.step);
        markInitialized();
        emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL), static_cast<uint8_t>(stepVar));
        
        // --- Runtime check: step != 0 ---
        emitBytes(static_cast<uint8_t>(OpCode::LOAD_LOCAL), static_cast<uint8_t>(stepVar));
        emitOp(OpCode::LOAD_ZERO);
        emitOp(OpCode::EQUAL);
        size_t stepZeroJump = emitJump(OpCode::JUMP_IF_FALSE);
        
        int errStrIdx = (int)makeConstant(Constant("Step cannot be 0 in repeat loop"));
        emitOp(OpCode::LOAD_CONST);
        emitBytes(static_cast<uint8_t>((errStrIdx >> 8) & 0xFF), static_cast<uint8_t>(errStrIdx & 0xFF));
        emitOp(OpCode::THROW);

        patchJump(stepZeroJump);

        startLoop();
        size_t loopStart = currentChunk().code.size();
        loopStack.back().start = loopStart;

        // Condition: (step > 0) ? (loopVar <= endVar) : (loopVar >= endVar)
        emitBytes(static_cast<uint8_t>(OpCode::LOAD_LOCAL), static_cast<uint8_t>(stepVar));
        emitOp(OpCode::LOAD_ZERO);
        emitOp(OpCode::GREATER);
        size_t stepJump = emitJump(OpCode::JUMP_IF_FALSE);

        // Positive step: endVar >= loopVar
        emitBytes(static_cast<uint8_t>(OpCode::LOAD_LOCAL), static_cast<uint8_t>(endVar));
        emitBytes(static_cast<uint8_t>(OpCode::LOAD_LOCAL), static_cast<uint8_t>(loopVar));
        emitOp(OpCode::GREATER_EQ); 
        size_t endStepJump = emitJump(OpCode::JUMP);

        patchJump(stepJump);

        // Negative step: loopVar >= endVar
        emitBytes(static_cast<uint8_t>(OpCode::LOAD_LOCAL), static_cast<uint8_t>(loopVar));
        emitBytes(static_cast<uint8_t>(OpCode::LOAD_LOCAL), static_cast<uint8_t>(endVar));
        emitOp(OpCode::GREATER_EQ); 

        patchJump(endStepJump);
        
        size_t exitJump = emitJump(OpCode::JUMP_IF_FALSE);

        compileStmt(stmt.body);

        // loopVar += stepVar
        emitBytes(static_cast<uint8_t>(OpCode::LOAD_LOCAL), static_cast<uint8_t>(loopVar));
        emitBytes(static_cast<uint8_t>(OpCode::LOAD_LOCAL), static_cast<uint8_t>(stepVar));
        emitOp(OpCode::ADD);
        emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL), static_cast<uint8_t>(loopVar));
        emitOp(OpCode::POP);

        emitLoop(loopStart);
        patchJump(exitJump);

        for (size_t breakOffset : loopStack.back().breaks) patchJump(breakOffset);
        endLoop();
    }
    endScope();
}

void BytecodeCompiler::compileGet(const GetStmt& stmt) {
    // Push the iterable and convert it to an iterator object
    compileExpr(stmt.iterable);
    if (!stmt.valueVariable.empty()) {
        emitOp(OpCode::GET_DICT_ITER);
    } else {
        emitOp(OpCode::GET_ITER);
    }

    beginScope();

    // Hidden local: the iterator  (slot N)
    size_t iterVar = addLocal("<iter>");
    current->locals.back().isStackResident = false;
    markInitialized();
    // GET_ITER leaves the iterator on the stack; store it into iterVar slot.
    emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL),
              static_cast<uint8_t>(iterVar));
    emitOp(OpCode::POP);

    // Loop variable that receives each element (or key)
    size_t loopVar = addLocal(stmt.variable);
    current->locals.back().isStackResident = false;
    markInitialized();
    emitOp(OpCode::LOAD_NIL);
    emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL),
              static_cast<uint8_t>(loopVar));
    emitOp(OpCode::POP);
    
    // Optional second variable for value in dict iteration
    size_t loopValueVar = 0;
    if (!stmt.valueVariable.empty()) {
        loopValueVar = addLocal(stmt.valueVariable);
        current->locals.back().isStackResident = false;
        markInitialized();
        emitOp(OpCode::LOAD_NIL);
        emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL),
                  static_cast<uint8_t>(loopValueVar));
        emitOp(OpCode::POP);
    }

    startLoop();
    size_t loopStart = currentChunk().code.size();
    loopStack.back().start = loopStart;

    emitBytes(static_cast<uint8_t>(OpCode::LOAD_LOCAL),
              static_cast<uint8_t>(iterVar));
    size_t exitJump = emitJump(OpCode::ITER_NEXT);

    if (!stmt.valueVariable.empty()) {
        // We have [key, value] on the stack. Destructure it.
        emitOp(OpCode::DUP); // [array, array]
        
        // key = array[0]
        emitOp(OpCode::LOAD_ZERO);
        emitOp(OpCode::INDEX_GET); // [array, key]
        emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL), static_cast<uint8_t>(loopVar));
        emitOp(OpCode::POP); // [array]
        
        // value = array[1]
        emitOp(OpCode::LOAD_ONE);
        emitOp(OpCode::INDEX_GET); // [value]
        emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL), static_cast<uint8_t>(loopValueVar));
        emitOp(OpCode::POP); // []
    } else {
        // Store the value that ITER_NEXT left on the stack into loopVar
        emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL),
                  static_cast<uint8_t>(loopVar));
        emitOp(OpCode::POP);
    }

    compileStmt(stmt.body);
    emitLoop(loopStart);

    patchJump(exitJump);

    // Retrieve loop context again as the stack may have reallocated
    for (size_t breakOffset : loopStack.back().breaks) patchJump(breakOffset);
    endLoop();
    endScope();
}

void BytecodeCompiler::compileMatch(const MatchStmt& stmt) {
    compileExpr(stmt.subject);

    std::vector<size_t> endJumps;

    // Built-in primitive type names for type-match arms
    static const std::unordered_set<std::string> primitiveTypes = {
        "Integer", "Float", "Number", "String", "Boolean", "Array", "Dictionary", "Nil"
    };

    for (const auto& arm : stmt.arms) {
        if (!arm.pattern) {
            // Default arm ('other')
            emitOp(OpCode::POP);
            compileStmt(arm.body);
            endJumps.push_back(emitJump(OpCode::JUMP));
        } else {
            // Detect type-match pattern: a bare uppercase identifier like String, Integer, User
            bool isTypePattern = false;
            std::string typeName;
            if (auto* idPtr = std::get_if<IdentifierExpr*>(&arm.pattern->variant)) {
                const std::string& name = (*idPtr)->name;
                if (!name.empty() && std::isupper((unsigned char)name[0])) {
                    isTypePattern = true;
                    typeName = name;
                }
            }

            emitOp(OpCode::DUP);

            if (isTypePattern) {
                // Emit: dup value instanceof TypeName
                // IS_INSTANCE_OF pops: [className_string, value] → bool
                // (It now handles primitive type strings like "Number", "Boolean" via VM changes)
                size_t nameIdx = identifierConstant(typeName);
                emitOp(OpCode::LOAD_CONST);
                emitBytes(static_cast<uint8_t>((nameIdx >> 8) & 0xFF),
                          static_cast<uint8_t>(nameIdx & 0xFF));
                emitOp(OpCode::IS_INSTANCE_OF);    // [..., bool]
            } else {
                // Regular value equality match
                compileExpr(arm.pattern);
                emitOp(OpCode::EQUAL);
            }

            size_t nextArmJump = emitJump(OpCode::JUMP_IF_FALSE);

            // Match: pop the subject duplicate
            emitOp(OpCode::POP);

            compileStmt(arm.body);
            endJumps.push_back(emitJump(OpCode::JUMP));

            // Not a match, jump here
            patchJump(nextArmJump);
        }
    }

    // Pop the subject at the very end in case nothing matched
    emitOp(OpCode::POP);

    for (size_t jump : endJumps) {
        patchJump(jump);
    }
}


void BytecodeCompiler::emitClosure(const TaskStmt& stmt, bool isMethod) {
    if (!current) {
        errorAt("emitClosure: no active compiler", currentLine);
        return;
    }

    // Compile the function and record it as a nested function of the
    // enclosing BytecodeFunction so the VM index is stable.
    size_t nestedIdx = current->function->nestedFunctions.size();
    BytecodeFunctionPtr func = compileFunction(stmt, stmt.name);
    func->isMethod = isMethod;
    current->function->nestedFunctions.push_back(func);

    // Emit CLOSURE <nestedIdx> (16-bit) + upvalue descriptors
    emitOp(OpCode::CLOSURE);
    emitBytes(static_cast<uint8_t>((nestedIdx >> 8) & 0xFF),
              static_cast<uint8_t>(nestedIdx & 0xFF));
    for (const auto& uv : func->upvalues) {
        emitByte(uv.type == Upvalue::Type::LOCAL ? 1 : 0);
        emitByte(static_cast<uint8_t>(uv.index));
    }
}

void BytecodeCompiler::compileTask(const TaskStmt& stmt) {
    // 1. Load user-defined decorators from innermost to outermost
    for (auto it = stmt.userDecorators.rbegin(); it != stmt.userDecorators.rend(); ++it) {
        compileExpr(*it);
    }

    emitClosure(stmt);

    // 2. Apply user-defined decorators by calling them with the closure
    for (size_t i = 0; i < stmt.userDecorators.size(); ++i) {
        emitOp(OpCode::CALL);
        emitByte(1); // 1 argument
    }

    // Store into variable (local or global)
    int local = resolveLocal(stmt.name);
    if (local != -1) {
        emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL),
                  static_cast<uint8_t>(local));
    } else if (current->scopeDepth > 0) {
        // In namespaced modules (depth 1) or nested functions, tasks are locals
        size_t slot = addLocal(stmt.name);
        markInitialized();
        emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL),
                  static_cast<uint8_t>(slot));
    } else {
        // Global task — allocate a slot
        uint16_t slot = globalSlotFor(stmt.name);
        emitOp(OpCode::STORE_GLOBAL_SLOT);
        emitBytes(static_cast<uint8_t>((slot >> 8) & 0xFF),
                  static_cast<uint8_t>(slot & 0xFF));
    }
    emitOp(OpCode::POP);
}

void BytecodeCompiler::compileGive(const GiveStmt& stmt) {
    // ---- Tail Call Optimization ----
    // If the return value is a direct call to the current function, and there are
    // no postconditions or caching side-effects, emit TAIL_CALL instead of CALL+RETURN.
    bool isTailCall = false;
    if (stmt.value && current && !current->isCached
        && (!currentEnsuresClauses || currentEnsuresClauses->empty())) {
        if (auto* callPtr = std::get_if<CallExpr*>(&stmt.value->variant)) {
            const CallExpr& call = **callPtr;
            // Check if callee is an identifier matching the current function name
            if (auto* idPtr = std::get_if<IdentifierExpr*>(&call.callee->variant)) {
                if ((*idPtr)->name == current->function->name) {
                    // Compile callee (the function itself)
                    compileExpr(call.callee);
                    // Compile arguments
                    for (const auto& arg : call.arguments) {
                        compileExpr(arg);
                    }
                    // Emit TAIL_CALL with argument count
                    emitOp(OpCode::TAIL_CALL);
                    emitByte(static_cast<uint8_t>(call.arguments.size()));
                    // TAIL_CALL handles its own return — we're done
                    isTailCall = true;
                }
            }
        }
    }

    if (!isTailCall) {
        if (stmt.value) {
            compileExpr(stmt.value);
        } else {
            emitOp(OpCode::LOAD_NIL);
        }

        if (current && current->isCached) {
            size_t nameIdx = identifierConstant(current->function->name);
            emitOp(OpCode::STORE_CACHED_RESULT);
            emitBytes(static_cast<uint8_t>((nameIdx >> 8) & 0xFF),
                      static_cast<uint8_t>(nameIdx & 0xFF));
        }

        // ---- Design-by-Contract: ensures (postconditions) ----
        if (!disableContracts && currentEnsuresClauses && !currentEnsuresClauses->empty()) {
            size_t resultSlot = addLocal("__result__");
            markInitialized();
            emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL),
                      static_cast<uint8_t>(resultSlot));
            emitOp(OpCode::POP);

            size_t resultAliasSlot = addLocal("result");
            markInitialized();
            emitBytes(static_cast<uint8_t>(OpCode::LOAD_LOCAL),
                      static_cast<uint8_t>(resultSlot));
            emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL),
                      static_cast<uint8_t>(resultAliasSlot));
            emitOp(OpCode::POP);

            compileContractChecks(*currentEnsuresClauses, false);

            emitBytes(static_cast<uint8_t>(OpCode::LOAD_LOCAL),
                      static_cast<uint8_t>(resultSlot));
        }

        emitReturn();
    }
}

// Compile precondition (requires) or postcondition (ensures) checks.
// For each clause: evaluate condition, if false throw a contract error.
void BytecodeCompiler::compileContractChecks(const std::vector<std::pair<ExprPtr, std::string>>& clauses, bool isPrecondition) {
    if (disableContracts) return;
    const std::string prefix = isPrecondition ? "Precondition failed" : "Postcondition failed";
    
    for (const auto& [condition, message] : clauses) {
        // We need to compile the condition but intercept any `old(expr)` calls.
        // We do this by walking the expression and substituting old(expr) nodes
        // with LOAD_LOCAL for the pre-captured slot.
        // For now, we compile the expression directly — old() calls will try to
        // call a function named "old" which we intercept in compileCall via oldCaptures.
        compileExpr(condition);
        
        // If condition is truthy, skip the throw
        size_t skipThrow = emitJump(OpCode::JUMP_IF_TRUE);
        
        // Build error message
        std::string errorMsg = prefix;
        if (!message.empty()) {
            errorMsg += ": " + message;
        }
        
        // Emit throw with error message string
        emitConstant(Value(errorMsg));
        emitOp(OpCode::THROW);
        
        patchJump(skipThrow);
    }
}

void BytecodeCompiler::compileEscape(const EscapeStmt& /*stmt*/) {
    if (loopStack.empty()) {
        errorAt("'escape' outside of loop", currentLine);
        return;
    }
    emitBreak();
}

void BytecodeCompiler::compileSkip(const SkipStmt& /*stmt*/) {
    if (loopStack.empty()) {
        errorAt("'skip' outside of loop", currentLine);
        return;
    }
    emitContinue();
}

void BytecodeCompiler::compileExport(const ExportStmt& stmt) {
    // Compile the inner declaration normally
    size_t localsBefore = current->locals.size();
    compileStmt(stmt.inner);
    // Mark all newly introduced locals as exported
    for (size_t i = localsBefore; i < current->locals.size(); i++) {
        current->locals[i].exported = true;
    }
}

void BytecodeCompiler::compileUse(const UseStmt& stmt) {
    std::string path = stmt.path;
    std::string absolutePath = path;
    
    // Resolve relative to current file
    if (!currentFile.empty() && currentFile != "repl" && currentFile != "main") {
        std::string dir = getDirectoryName(currentFile);
        if (dir != ".") {
            absolutePath = dir + "/" + path;
        }
    }
    
    std::string source;
    
    // Shared resolver so the compiler, bundler and package manager all agree on
    // where the stdlib lives (see src/utils/EzLibPath.h).
    std::string ezlibBase = ezLibBase();
    
    // 1. Check Virtual File System first
    bool foundInVFS = false;
    std::string vfsSearchPath = absolutePath;
    
    // Normalize path separators for VFS check
    std::replace(vfsSearchPath.begin(), vfsSearchPath.end(), '\\', '/');
    if (vfsSearchPath.size() > 2 && vfsSearchPath[1] == ':') {
        // If it's an absolute windows path, we can't easily match VFS, but usually VFS paths are relative like "lib/gui.ez"
        // In the packager, we store them as relative paths
    }
    
    // Check various common formats in VFS
    if (virtualFileSystem.count(path)) {
        source = virtualFileSystem[path];
        absolutePath = path;
        foundInVFS = true;
    } else if (virtualFileSystem.count(path + ".ez")) {
        source = virtualFileSystem[path + ".ez"];
        absolutePath = path + ".ez";
        foundInVFS = true;
    } else if (virtualFileSystem.count("lib/" + path + ".ez")) {
        source = virtualFileSystem["lib/" + path + ".ez"];
        absolutePath = "lib/" + path + ".ez";
        foundInVFS = true;
    } else if (virtualFileSystem.count("lib/" + path + "/main.ez")) {
        source = virtualFileSystem["lib/" + path + "/main.ez"];
        absolutePath = "lib/" + path + "/main.ez";
        foundInVFS = true;
    } else if (virtualFileSystem.count(ezlibBase + path)) {
        source = virtualFileSystem[ezlibBase + path];
        absolutePath = ezlibBase + path;
        foundInVFS = true;
    } else if (virtualFileSystem.count(ezlibBase + path + ".ez")) {
        source = virtualFileSystem[ezlibBase + path + ".ez"];
        absolutePath = ezlibBase + path + ".ez";
        foundInVFS = true;
    } else if (virtualFileSystem.count(ezlibBase + path + "/main.ez")) {
        source = virtualFileSystem[ezlibBase + path + "/main.ez"];
        absolutePath = ezlibBase + path + "/main.ez";
        foundInVFS = true;
    }
    
    if (!foundInVFS) {
        std::ifstream file(absolutePath);
        if (!file.is_open()) {
            // Try exactly as typed
            file.open(path);
            // A relative path to a local module written WITHOUT the .ez
            // extension, e.g. `use "Test/lib_a"` for ./Test/lib_a.ez. Every
            // other search location below appends .ez, but the raw relative path
            // never did -- so a local module could only be imported by spelling
            // out the extension.
            std::string rawEzPath = path;
            if (!file.is_open() && path.find(".ez") == std::string::npos) {
                rawEzPath = path + ".ez";
                file.open(rawEzPath);
            }

            if (file.is_open()) {
                // rawEzPath == path unless the .ez fallback above opened it.
                absolutePath = rawEzPath;
            } else {
                // Try local lib/ directory
                std::string localLibPath = "lib/" + path;
                file.open(localLibPath);
                if (file.is_open()) {
                    absolutePath = localLibPath;
                } else {
                    // Try standard lib path
                    std::string libPath = ezlibBase + path;
                    file.open(libPath);
                    if (file.is_open()) {
                        absolutePath = libPath;
                    } else if (fs::is_directory(libPath)) {
                        // It's a directory, look for [packageName].ez or main.ez
                        std::string pkgEz = libPath + "/" + path + ".ez";
                        file.open(pkgEz);
                        if (file.is_open()) {
                            absolutePath = pkgEz;
                        } else {
                            std::string mainEz = libPath + "/main.ez";
                            file.open(mainEz);
                            if (file.is_open()) {
                                absolutePath = mainEz;
                            } else {
                                errorAt("Could not find entry point in module directory '" + libPath + "'", currentLine);
                                return;
                            }
                        }
                    } else {
                        // Try .ez extension
                        std::string ezPath = (libPath.find(".ez") == std::string::npos) ? libPath + ".ez" : libPath;
                        file.open(ezPath);
                        if (file.is_open()) {
                            absolutePath = ezPath;
                        } else {
                            // Try local .ez extension in lib
                            std::string localEzPath = (localLibPath.find(".ez") == std::string::npos) ? localLibPath + ".ez" : localLibPath;
                            file.open(localEzPath);
                            if (file.is_open()) {
                                absolutePath = localEzPath;
                            } else {
                                errorAt("Could not find module '" + path + "'", currentLine);
                                return;
                            }
                        }
                    }
                }
            }
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        source = buffer.str();
    }
    
    if (compilingModules.count(absolutePath)) {
        errorAt("Circular dependency detected when importing '" + absolutePath + "'", currentLine);
        return;
    }
    compilingModules.insert(absolutePath);
    
    struct ModuleCleaner {
        std::string path;
        std::unordered_set<std::string>& set;
        ModuleCleaner(std::string p, std::unordered_set<std::string>& s) : path(p), set(s) {}
        ~ModuleCleaner() { set.erase(path); }
    } cleaner(absolutePath, compilingModules);
    
    std::vector<StmtPtr> statements;
    if (astCache.count(absolutePath)) {
        statements = astCache[absolutePath];
    } else {
        // Register the module source so runtimeError() can show snippets from it
        {
            extern void EZ_RegisterSource(const std::string&, const std::string&);
            EZ_RegisterSource(absolutePath, source);
        }

        // Compile the imported file
        Lexer lexer(source, absolutePath);
        auto tokens = lexer.tokenize();
        if (lexer.hasError()) {
            errorAt("Lexer error in module '" + absolutePath + "'", currentLine);
            return;
        }
        
        Parser parser(tokens, arena);
        statements = parser.parse();
        if (parser.hasError()) {
            errorAt("Parser error in module '" + absolutePath + "'", currentLine);
            return;
        }
        astCache[absolutePath] = statements;
    }
    
    // Handle namespacing if an alias is provided
    std::string alias = stmt.alias;
    if (alias.empty()) {
        alias = "*";
    }

    std::string execFlag = "__module_cache_" + std::to_string(std::hash<std::string>{}(absolutePath));
    size_t flagIdx = identifierConstant(execFlag);

    if (alias == "*") {
        // Global import: splash all statements into current function if not executed
        emitOp(OpCode::HAS_GLOBAL);
        emitBytes(static_cast<uint8_t>((flagIdx >> 8) & 0xFF), static_cast<uint8_t>(flagIdx & 0xFF));
        size_t skipExec = emitJump(OpCode::JUMP_IF_TRUE);
        
        size_t trueIdx = current->function->chunk.addConstant(Constant(true));
        emitOp(OpCode::LOAD_CONST);
        emitBytes(static_cast<uint8_t>((trueIdx >> 8) & 0xFF), static_cast<uint8_t>(trueIdx & 0xFF));
        // Cache flag in globalEnv via string-based STORE_GLOBAL (dynamic runtime flag)
        emitOp(OpCode::STORE_GLOBAL);
        emitBytes(static_cast<uint8_t>((flagIdx >> 8) & 0xFF), static_cast<uint8_t>(flagIdx & 0xFF));
        emitOp(OpCode::POP);

        for (const auto& s : statements) {
            compileStmt(s);
        }
        
        patchJump(skipExec);
    } else {
        // Namespaced import: wrap in a module closure and return a dictionary of locals
        std::unique_ptr<Compiler> moduleCompiler(new Compiler(alias + "_module", 0, current));
        Compiler* previous = current;
        current = moduleCompiler.get();

        // Force module top-level to be locals by starting at depth 1
        current->scopeDepth = 1;
        current->isHarvesting = true;
        
        // Reserve slot 0 for the module closure itself (standard VM frame layout)
        addLocal("");
        markInitialized();

        // Compile module body
        for (const auto& s : statements) {
            compileStmt(s);
        }

        // Harvest locals into a dictionary for export
        // Strategy: if the path ends in .ez (file inclusion), export all
        // Otherwise (module import by name), only export marked ones.
        bool isFileInclusion = (stmt.path.size() >= 3 &&
            stmt.path.rfind(".ez") == stmt.path.size() - 3);

        emitOp(OpCode::MAKE_DICT);
        emitByte(0); // Start with empty dict

        for (const auto& local : current->locals) {
            if (local.depth == 1 && !local.name.empty()) {
                // Skip non-exported locals in module (non-.ez) imports
                if (!isFileInclusion && !local.exported) continue;
                
                // DUP dict, load local, store property
                emitOp(OpCode::DUP);
                
                // Find where the local is (using resolveLocal to be safe)
                int slot = resolveLocal(local.name);
                emitOp(OpCode::LOAD_LOCAL);
                emitByte(static_cast<uint8_t>(slot));
                
                emitOp(OpCode::INTERCEPTED_STORE_PROPERTY);
                size_t propIdx = identifierConstant(local.name);
                emitBytes(static_cast<uint8_t>((propIdx >> 8) & 0xFF),
                          static_cast<uint8_t>(propIdx & 0xFF));
                size_t icIdx = current->function->chunk.icEntries.size();
                current->function->chunk.icEntries.push_back(ICCacheEntry{});
                emitBytes(static_cast<uint8_t>((icIdx >> 8) & 0xFF), static_cast<uint8_t>(icIdx & 0xFF));
                emitOp(OpCode::POP); // pop result of store_property
            }
        }
        
        emitOp(OpCode::RETURN);
        
        // Finalize module function
        BytecodeFunctionPtr moduleFunc = current->function;
        moduleFunc->localCount = current->locals.size();
        moduleFunc->upvalueCount = current->upvalues.size();
        moduleFunc->upvalues = current->upvalues;
        compiledFunctions.push_back(moduleFunc);

        // Restore compiler to parent
        current = previous;

        // Register the module function as a nested function of the parent compiler
        size_t nestedIdx = current->function->nestedFunctions.size();
        current->function->nestedFunctions.push_back(moduleFunc);

        emitOp(OpCode::HAS_GLOBAL);
        emitBytes(static_cast<uint8_t>((flagIdx >> 8) & 0xFF), static_cast<uint8_t>(flagIdx & 0xFF));
        size_t skipExec = emitJump(OpCode::JUMP_IF_TRUE);

        // In the parent scope, create the closure, call it, and store the result in the cache
        emitOp(OpCode::CLOSURE);
        emitBytes(static_cast<uint8_t>((nestedIdx >> 8) & 0xFF),
                  static_cast<uint8_t>(nestedIdx & 0xFF));
        
        // Emit upvalue info for the closure (it might capture from outer scope!)
        for (const auto& uv : moduleFunc->upvalues) {
            emitByte(uv.type == Upvalue::Type::LOCAL ? 1 : 0);
            emitByte(static_cast<uint8_t>(uv.index));
        }

        emitOp(OpCode::CALL);
        emitByte(0); // 0 args

        // Store to global cache
        emitOp(OpCode::STORE_GLOBAL);
        emitBytes(static_cast<uint8_t>((flagIdx >> 8) & 0xFF), static_cast<uint8_t>(flagIdx & 0xFF));
        emitOp(OpCode::POP);
        
        patchJump(skipExec);
        
        // Load from global cache and store into the requested alias
        emitOp(OpCode::LOAD_GLOBAL);
        emitBytes(static_cast<uint8_t>((flagIdx >> 8) & 0xFF), static_cast<uint8_t>(flagIdx & 0xFF));

        size_t aliasIdx = identifierConstant(alias);
        if (current->scopeDepth > 0) {
            size_t slot = addLocal(alias);
            markInitialized();
            emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL),
                      static_cast<uint8_t>(slot));
            emitOp(OpCode::POP);
        } else {
            uint16_t aliasSlot = globalSlotFor(alias);
            emitOp(OpCode::STORE_GLOBAL_SLOT);
            emitBytes(static_cast<uint8_t>((aliasSlot >> 8) & 0xFF),
                      static_cast<uint8_t>(aliasSlot & 0xFF));
            emitOp(OpCode::POP);
        }
    }
}

void BytecodeCompiler::compileModel(const ModelStmt& stmt) {
    // 0. Load user-defined decorators from innermost to outermost
    for (auto it = stmt.userDecorators.rbegin(); it != stmt.userDecorators.rend(); ++it) {
        compileExpr(*it);
    }

    // Save previous class context
    std::string savedClass = current->currentClass;
    std::string savedParent = current->currentParentClass;
    current->currentClass = stmt.name;
    current->currentParentClass = stmt.parentName;

    // 1. Push Parent class (or Nil)
    if (stmt.parentName.empty()) {
        emitOp(OpCode::LOAD_NIL);
    } else {
        auto parentIt = globalSlots.find(stmt.parentName);
        if (parentIt != globalSlots.end()) {
            uint16_t slot = parentIt->second;
            emitOp(OpCode::LOAD_GLOBAL_SLOT);
            emitBytes(static_cast<uint8_t>((slot >> 8) & 0xFF),
                      static_cast<uint8_t>(slot & 0xFF));
        } else {
            size_t parentIdx = identifierConstant(stmt.parentName);
            emitOp(OpCode::LOAD_GLOBAL);
            emitBytes(static_cast<uint8_t>((parentIdx >> 8) & 0xFF),
                      static_cast<uint8_t>(parentIdx & 0xFF));
        }
    }
    
    int memberCount = 0;
    std::vector<std::pair<std::string, std::string>> propertyTypes;
    
    // 2. Synthesize "init" method if there's a constructor
    // Note: Always create 'init' if there are params, even if body is empty
    if (!stmt.initParams.empty() || !stmt.initBody.empty()) {
        // Push name
        size_t initNameIdx = identifierConstant("init");
        emitOp(OpCode::LOAD_CONST);
        emitBytes(static_cast<uint8_t>((initNameIdx >> 8) & 0xFF),
                  static_cast<uint8_t>(initNameIdx & 0xFF));
        
        std::vector<std::string> params = {"self"};
        params.insert(params.end(), stmt.initParams.begin(), stmt.initParams.end());
        std::vector<ExprPtr> defaults = {nullptr}; // for 'self'
        defaults.insert(defaults.end(), stmt.initDefaultValues.begin(), stmt.initDefaultValues.end());
        TaskStmt initTask("init", params, std::vector<TypeASTPtr>(params.size(), arena.allocate<TypeAST>("Any")), defaults, nullptr, stmt.initBody);
        emitClosure(initTask, true); // Pushes closure
        
        // Push isStatic flag (false for constructor)
        emitOp(OpCode::LOAD_FALSE);
        
        // Push isPublic flag (true for constructor)
        emitOp(OpCode::LOAD_TRUE);
        
        memberCount++;
    }
    
    // 3. Push other members
    for (const auto& member : stmt.members) {
        if (!member.isMethod && !member.isStatic) {
            std::string typeStr = member.typeHint ? member.typeHint->baseType : "Any";
            propertyTypes.push_back({member.name, typeStr});
        }
    
        // Push name
        size_t memberNameIdx = identifierConstant(member.name);
        emitOp(OpCode::LOAD_CONST);
        emitBytes(static_cast<uint8_t>((memberNameIdx >> 8) & 0xFF),
                  static_cast<uint8_t>(memberNameIdx & 0xFF));
        
        if (member.isMethod) {
            std::vector<std::string> params;
            std::vector<ExprPtr> defaults;
            if (!member.isStatic) {
                params.push_back("self");
                defaults.push_back(nullptr);
            }
            params.insert(params.end(), member.params.begin(), member.params.end());
            defaults.insert(defaults.end(), member.defaultValues.begin(), member.defaultValues.end());
            TaskStmt methodTask(member.name, params, std::vector<TypeASTPtr>(params.size(), arena.allocate<TypeAST>("Any")), defaults, nullptr, member.body, false, member.isAsync);
            methodTask.isCached = member.isCached;
            emitClosure(methodTask, true); // Pushes closure
        } else {
            if (member.initializer) {
                compileExpr(member.initializer);
            } else {
                emitOp(OpCode::LOAD_NIL);
            }
        }
        
        // Push isStatic flag
        emitOp(member.isStatic ? OpCode::LOAD_TRUE : OpCode::LOAD_FALSE);
        
        // Push isPublic flag
        emitOp(member.visibility == MemberVisibility::PUBLIC ? OpCode::LOAD_TRUE : OpCode::LOAD_FALSE);
        
        memberCount++;
    }
    
    // 4. Inject __properties__ static dictionary
    {
        size_t propsNameIdx = identifierConstant("__properties__");
        emitOp(OpCode::LOAD_CONST);
        emitBytes(static_cast<uint8_t>((propsNameIdx >> 8) & 0xFF),
                  static_cast<uint8_t>(propsNameIdx & 0xFF));

        for (const auto& prop : propertyTypes) {
            size_t keyIdx = identifierConstant(prop.first);
            emitOp(OpCode::LOAD_CONST);
            emitBytes(static_cast<uint8_t>((keyIdx >> 8) & 0xFF),
                      static_cast<uint8_t>(keyIdx & 0xFF));

            size_t valIdx = identifierConstant(prop.second);
            emitOp(OpCode::LOAD_CONST);
            emitBytes(static_cast<uint8_t>((valIdx >> 8) & 0xFF),
                      static_cast<uint8_t>(valIdx & 0xFF));
        }
        
        emitOp(OpCode::MAKE_DICT);
        emitByte(static_cast<uint8_t>(propertyTypes.size()));
        
        // isStatic = true
        emitOp(OpCode::LOAD_TRUE);
        
        // isPublic = true
        emitOp(OpCode::LOAD_TRUE);
        
        memberCount++;
    }

    // 4.1 Inject __name__ static string
    {
        size_t nameNameIdx = identifierConstant("__name__");
        emitOp(OpCode::LOAD_CONST);
        emitBytes(static_cast<uint8_t>((nameNameIdx >> 8) & 0xFF),
                  static_cast<uint8_t>(nameNameIdx & 0xFF));
        
        size_t valIdx = identifierConstant(stmt.name);
        emitOp(OpCode::LOAD_CONST);
        emitBytes(static_cast<uint8_t>((valIdx >> 8) & 0xFF),
                  static_cast<uint8_t>(valIdx & 0xFF));
        emitOp(OpCode::LOAD_TRUE); // isStatic
        emitOp(OpCode::LOAD_TRUE); // isPublic
        memberCount++;
    }

    // 5. Push interfaces onto stack
    for (const auto& interfaceName : stmt.interfaces) {
        auto ifIt = globalSlots.find(interfaceName);
        if (ifIt != globalSlots.end()) {
            uint16_t slot = ifIt->second;
            emitOp(OpCode::LOAD_GLOBAL_SLOT);
            emitBytes(static_cast<uint8_t>((slot >> 8) & 0xFF),
                      static_cast<uint8_t>(slot & 0xFF));
        } else {
            size_t idx = identifierConstant(interfaceName);
            emitOp(OpCode::LOAD_GLOBAL);
            emitBytes(static_cast<uint8_t>((idx >> 8) & 0xFF),
                      static_cast<uint8_t>(idx & 0xFF));
        }
    }

    // 6. Push behavior metadata onto stack (popped by MAKE_CLASS handler)
    // Push audited flag
    emitOp(stmt.audited ? OpCode::LOAD_TRUE : OpCode::LOAD_FALSE);
    // Push snapshot flag
    emitOp(stmt.snapshot ? OpCode::LOAD_TRUE : OpCode::LOAD_FALSE);
    // Push persist path (empty = not persistent)
    size_t persistIdx = identifierConstant(stmt.persistPath);
    emitOp(OpCode::LOAD_CONST);
    emitBytes(static_cast<uint8_t>((persistIdx >> 8) & 0xFF),
              static_cast<uint8_t>(persistIdx & 0xFF));
    // Push validator metadata (field, rule, param, message) for each @validate rule
    int validatorCount = 0;
    for (const auto& member : stmt.members) {
        if (!member.isMethod) {
            for (const auto& vr : member.validators) {
                size_t fnIdx = identifierConstant(member.name);
                emitOp(OpCode::LOAD_CONST);
                emitBytes(static_cast<uint8_t>((fnIdx >> 8) & 0xFF), static_cast<uint8_t>(fnIdx & 0xFF));
                size_t rIdx = identifierConstant(vr.ruleName);
                emitOp(OpCode::LOAD_CONST);
                emitBytes(static_cast<uint8_t>((rIdx >> 8) & 0xFF), static_cast<uint8_t>(rIdx & 0xFF));
                if (vr.param) { compileExpr(vr.param); } else { emitOp(OpCode::LOAD_NIL); }
                size_t mIdx = identifierConstant(vr.message);
                emitOp(OpCode::LOAD_CONST);
                emitBytes(static_cast<uint8_t>((mIdx >> 8) & 0xFF), static_cast<uint8_t>(mIdx & 0xFF));
                validatorCount++;
            }
        }
    }

    // 7. Emit MAKE_CLASS nameIdx, memberCount, interfaceCount, validatorCount
    if (memberCount > 255) errorAt("Too many members in model (max 255)", currentLine);
    if (stmt.interfaces.size() > 255) errorAt("Too many interfaces in model (max 255)", currentLine);
    if (validatorCount > 255) errorAt("Too many validators in model (max 255)", currentLine);
    
    size_t classNameIdx = identifierConstant(stmt.name);
    emitOp(OpCode::MAKE_CLASS);
    emitBytes(static_cast<uint8_t>((classNameIdx >> 8) & 0xFF),
              static_cast<uint8_t>(classNameIdx & 0xFF));
    emitByte(static_cast<uint8_t>(memberCount));
    emitByte(static_cast<uint8_t>(stmt.interfaces.size()));
    emitByte(static_cast<uint8_t>(validatorCount));   // NEW: validator count

    // Apply user-defined decorators by calling them with the class object
    for (size_t i = 0; i < stmt.userDecorators.size(); ++i) {
        emitOp(OpCode::CALL);
        emitByte(1); // 1 argument
    }
    
    // 6. Store class in variable (global by default, local if in a module)
    if (current->scopeDepth > 0) {
        size_t slot = addLocal(stmt.name);
        markInitialized();
        emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL),
                  static_cast<uint8_t>(slot));
        // Do NOT emit POP! The local variable MUST remain on the stack.
    } else {
        uint16_t slot = globalSlotFor(stmt.name);
        emitOp(OpCode::STORE_GLOBAL_SLOT);
        emitBytes(static_cast<uint8_t>((slot >> 8) & 0xFF),
                  static_cast<uint8_t>(slot & 0xFF));
        emitOp(OpCode::POP);
    }

    // Restore class context
    current->currentClass = savedClass;
    current->currentParentClass = savedParent;
}

void BytecodeCompiler::compileInterface(const InterfaceStmt& stmt) {
    // Push method names as constants
    for (const auto& method : stmt.methods) {
        size_t methodIdx = identifierConstant(method.name);
        emitOp(OpCode::LOAD_CONST);
        emitBytes(static_cast<uint8_t>((methodIdx >> 8) & 0xFF),
                  static_cast<uint8_t>(methodIdx & 0xFF));
    }
    
    size_t nameIdx = identifierConstant(stmt.name);
    emitOp(OpCode::MAKE_INTERFACE);
    emitBytes(static_cast<uint8_t>((nameIdx >> 8) & 0xFF),
              static_cast<uint8_t>(nameIdx & 0xFF));
    emitByte(static_cast<uint8_t>(stmt.methods.size()));
    
    // Store in global
    uint16_t ifSlot = globalSlotFor(stmt.name);
    emitOp(OpCode::STORE_GLOBAL_SLOT);
    emitBytes(static_cast<uint8_t>((ifSlot >> 8) & 0xFF),
              static_cast<uint8_t>(ifSlot & 0xFF));
    emitOp(OpCode::POP);
}

void BytecodeCompiler::compileStatic(const StaticStmt& stmt) {
    // Mangle name to avoid global collisions and function collisions: __static_<id>_<funcName>_<varName>
    std::string mangledName = "__static_" + std::to_string(current->compilerId) + "_" + current->function->name + "_" + stmt.name;
    current->statics[stmt.name] = mangledName;
    // globalSlotFor() will allocate/reuse a slot on demand when the mangled name is first emitted
    
    size_t nameIdx = identifierConstant(mangledName);
    
    // 1. Check if global exists
    emitOp(OpCode::HAS_GLOBAL);
    emitBytes(static_cast<uint8_t>((nameIdx >> 8) & 0xFF),
              static_cast<uint8_t>(nameIdx & 0xFF));
    
    // 2. If it EXISTS (is true), jump over the initialization
    size_t skipInit = emitJump(OpCode::JUMP_IF_TRUE);
    
    // 3. Initialization (runs only if HAS_GLOBAL was false)
    compileExpr(stmt.initializer);
    uint16_t staticSlot = globalSlotFor(mangledName);
    emitOp(OpCode::STORE_GLOBAL_SLOT);
    emitBytes(static_cast<uint8_t>((staticSlot >> 8) & 0xFF),
              static_cast<uint8_t>(staticSlot & 0xFF));
    emitOp(OpCode::POP);
    
    // 4. Patch jump
    patchJump(skipInit);
}

void BytecodeCompiler::compileTry(const TryStmt& stmt) {
    bool hasFinally = stmt.finallyBlock != nullptr;
    bool hasCatch = !stmt.catchBlocks.empty();
    
    int pendingSlot = -1;
    if (hasFinally) {
        beginScope(); // Scope for the hidden pending exception variable
        emitOp(OpCode::LOAD_NIL);
        pendingSlot = addLocal("__pendingExc__" + std::to_string(current->locals.size()));
        current->locals.back().isStackResident = true; // It is on the stack right now
        markInitialized();
    }
    
    // Emit TRY_START with a placeholder jump offset to the catch handler.
    size_t tryStart = emitJump(OpCode::TRY_START);

    // Compile the try block
    compileStmt(stmt.tryBlock);
    emitOp(OpCode::TRY_END);

    // Jump over catch handlers to finally (or end)
    size_t afterCatch = emitJump(OpCode::JUMP);

    // Patch TRY_START to point here (the catch handler entry or finally for try-finally)
    patchJump(tryStart);

    std::vector<size_t> successJumps;

    if (hasCatch) {
        // At this point, the exception is on the stack: [exc]
        for (const auto& cb : stmt.catchBlocks) {
            size_t nextCatch = 0;
            
            if (!cb.typeName.empty()) {
                // Typed catch: check if instance of type
                emitOp(OpCode::DUP); // [exc, exc]
                emitConstant(Value(cb.typeName)); // [exc, exc, "Type"]
                emitOp(OpCode::IS_INSTANCE_OF); // [exc, bool]
                
                nextCatch = emitJump(OpCode::JUMP_IF_FALSE); // Jump to next catch if not this type
                
                // Matches! Bind and execute body
                beginScope();
                if (!cb.varName.empty()) {
                    int slot = addLocal(cb.varName);
                    current->locals.back().isStackResident = false;
                    markInitialized();
                    emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL),
                              static_cast<uint8_t>(slot));
                    emitOp(OpCode::POP);
                } else {
                    emitOp(OpCode::POP); // No variable bound, discard exception
                }
                compileStmt(cb.body);
                endScope();
                
                successJumps.push_back(emitJump(OpCode::JUMP)); // Jump to finally/end
                
                patchJump(nextCatch); // Point next catch check here
            } else {
                // Catch-all
                beginScope();
                if (!cb.varName.empty()) {
                    int slot = addLocal(cb.varName);
                    current->locals.back().isStackResident = false;
                    markInitialized();
                    emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL),
                              static_cast<uint8_t>(slot));
                    emitOp(OpCode::POP);
                } else {
                    emitOp(OpCode::POP); // No variable bound, discard exception
                }
                compileStmt(cb.body);
                endScope();
                
                successJumps.push_back(emitJump(OpCode::JUMP));
                
                break;
            }
        }
        
        // If we fall through here, no catch block matched (and no catch-all was present)
        if (hasFinally) {
            // Save exception as pending and jump to finally
            // The exception is still on the stack from the catch dispatch
            emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL), static_cast<uint8_t>(pendingSlot));
            emitOp(OpCode::POP);
            
            // Jump to finally
            size_t toFinally = emitJump(OpCode::JUMP);
            successJumps.push_back(toFinally);
        } else {
            // Re-throw the exception which is still on the stack
            emitOp(OpCode::THROW);
        }
    } else {
        // try-finally without catch: exception is on stack from TRY_START catch handler
        // Save exception as pending and fall through to finally
        emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL), static_cast<uint8_t>(pendingSlot));
        emitOp(OpCode::POP);
    }

    // Patch all successful catch handlers and afterCatch to jump here
    for (size_t jump : successJumps) {
        patchJump(jump);
    }
    patchJump(afterCatch);
    
    // Emit finally block if present
    if (hasFinally) {
        compileStmt(stmt.finallyBlock);
        
        // Load pending exception
        emitBytes(static_cast<uint8_t>(OpCode::LOAD_LOCAL), static_cast<uint8_t>(pendingSlot));
        
        // We want to throw it if it's not nil.
        // DUP pushes a copy.
        // JUMP_IF_NIL checks the copy, pops it, and jumps if nil.
        // If it was nil, we jump to skipRethrow, where we POP the original nil.
        // If it was NOT nil, it falls through to THROW, which pops and throws the exception!
        
        emitOp(OpCode::DUP);
        size_t skipRethrow = emitJump(OpCode::JUMP_IF_NIL);
        
        emitOp(OpCode::THROW); // Re-throw the pending exception
        
        patchJump(skipRethrow);
        emitOp(OpCode::POP); // Pop the nil or the exception
        
        endScope(); // End the hidden scope
    }
}

void BytecodeCompiler::compileThrow(const ThrowStmt& stmt) {
    compileExpr(stmt.expr);
    emitOp(OpCode::THROW);
}

// ============================================================================
// Scope Management
// ============================================================================

void BytecodeCompiler::beginScope() {
    current->scopeDepth++;
}

void BytecodeCompiler::endScope() {
    current->scopeDepth--;

    while (!current->locals.empty() &&
           current->locals.back().depth > current->scopeDepth) {
           
        // Update debug info endPC
        size_t debugIdx = current->locals.back().localVarInfoIdx;
        current->function->localVars[debugIdx].endPC = currentChunk().code.size();
           
        if (current->locals.back().isCaptured) {
            emitOp(OpCode::CLOSE_UPVALUE);
            emitByte(static_cast<uint8_t>(current->locals.size() - 1));
        }
        if (current->locals.back().isStackResident) {
            emitOp(OpCode::POP);
        }
        current->locals.pop_back();
    }
}

size_t BytecodeCompiler::addLocal(const std::string& name, bool isConst) {
    Local local;
    local.name    = name;
    local.depth   = current->scopeDepth;
    local.isCaptured = false;
    local.isConst = isConst;
    local.exported = false;  // Default: not exported, only visible inside module
    local.isStackResident = true; // Default to true (VarDecl, loop vars, params)
    
    local.startPC = currentChunk().code.size();
    
    // Add to function's debug info
    LocalVarInfo info;
    info.name = name;
    info.slot = current->locals.size();
    info.startPC = local.startPC;
    info.endPC = SIZE_MAX; // Will be updated on endScope, SIZE_MAX if it lives until function return
    
    current->function->localVars.push_back(info);
    local.localVarInfoIdx = current->function->localVars.size() - 1;
    
    current->locals.push_back(local);
    if (current->locals.size() > 256) {
        errorAt("Too many local variables in function (max 256)", currentLine);
    }
    
    if (current->locals.size() > current->maxLocals) {
        current->maxLocals = current->locals.size();
    }
    return current->locals.size() - 1;
}

int BytecodeCompiler::resolveLocal(const std::string& name) {

    for (int i = static_cast<int>(current->locals.size()) - 1; i >= 0; i--) {
        if (current->locals[i].name == name) return i;
    }
    return -1;
}

int BytecodeCompiler::resolveUpvalue(const std::string& name) {
    if (!current->enclosing) return -1;


    // Look for the variable as a local in the ENCLOSING compiler scope
    for (int i = static_cast<int>(current->enclosing->locals.size()) - 1;
         i >= 0; i--) {
        if (current->enclosing->locals[i].name == name) {
            current->enclosing->locals[i].isCaptured = true;
            return addUpvalue(static_cast<size_t>(i), Upvalue::Type::LOCAL);
        }
    }

    // Recurse: look for the name as an upvalue in the enclosing scope
    // (needed for functions nested more than one level deep)
    Compiler* savedCurrent = current;
    current = current->enclosing;
    int upval = resolveUpvalue(name);
    current = savedCurrent;

    if (upval != -1) {
        return addUpvalue(static_cast<size_t>(upval), Upvalue::Type::UPVALUE);
    }

    return -1;
}

std::string BytecodeCompiler::resolveStatic(const std::string& name) {
    Compiler* c = current;
    while (c != nullptr) {
        auto it = c->statics.find(name);
        if (it != c->statics.end()) return it->second;
        c = c->enclosing;
    }
    return "";
}

int BytecodeCompiler::addUpvalue(size_t index, Upvalue::Type type) {
    // Deduplicate: reuse existing upvalue for the same capture
    for (size_t i = 0; i < current->upvalues.size(); i++) {
        if (current->upvalues[i].index == index &&
            current->upvalues[i].type  == type) {
            return static_cast<int>(i);
        }
    }

    Upvalue uv;
    uv.index = index;
    uv.type  = type;
    current->upvalues.push_back(uv);
    current->function->upvalues.push_back(uv);
    current->function->upvalueCount = current->upvalues.size();
    return static_cast<int>(current->upvalues.size() - 1);
}

void BytecodeCompiler::markInitialized() {
    if (current->scopeDepth == 0) return;
    current->locals.back().depth = current->scopeDepth;
}

// ============================================================================
// Code Emission
// ============================================================================

void BytecodeCompiler::emitByte(uint8_t byte) {
    currentChunk().writeByte(byte, currentLine);
}

void BytecodeCompiler::emitBytes(uint8_t b1, uint8_t b2) {
    currentChunk().writeBytes(b1, b2, currentLine);
}

void BytecodeCompiler::emitOp(OpCode op) {
    currentChunk().writeOp(op, currentLine);
}

void BytecodeCompiler::emitConstant(const Constant& constant) {
    size_t idx = makeConstant(constant);
    if (idx > 65535) {
        errorAt("Too many constants in one chunk (max 65535)", currentLine);
        return;
    }
    emitOp(OpCode::LOAD_CONST);
    emitBytes(static_cast<uint8_t>((idx >> 8) & 0xFF),
              static_cast<uint8_t>(idx & 0xFF));
}

void BytecodeCompiler::emitConstant(const Value& value) {
    if (value.isNil())          { emitOp(OpCode::LOAD_NIL); return; }
    if (value.isBool())         { emitOp(value.asBool() ? OpCode::LOAD_TRUE : OpCode::LOAD_FALSE); return; }
    if (value.isInteger()) {
        long long i = value.asInteger();
        if (i == 0) { emitOp(OpCode::LOAD_ZERO); return; }
        if (i == 1) { emitOp(OpCode::LOAD_ONE);  return; }
        emitConstant(Constant(i)); return;
    }
    if (value.isFloat())        { emitConstant(Constant(value.asFloat()));  return; }
    if (value.isString())       { emitConstant(Constant(value.asString())); return; }
    emitOp(OpCode::LOAD_NIL);
}

size_t BytecodeCompiler::emitJump(OpCode jumpOp) {
    currentChunk().writeJump(jumpOp, currentLine);
    return currentChunk().code.size() - 4;  // offset of the 4 placeholder bytes
}

void BytecodeCompiler::patchJump(size_t offset) {
    currentChunk().patchJump(offset);
}

void BytecodeCompiler::emitLoop(size_t loopStart) {
    currentChunk().writeLoop(loopStart, currentLine);
}

void BytecodeCompiler::emitReturn() {
    emitOp(OpCode::RETURN);
}

// ============================================================================
// Helpers
// ============================================================================

size_t BytecodeCompiler::makeConstant(const Constant& constant) {
    return currentChunk().addConstant(constant);
}

size_t BytecodeCompiler::identifierConstant(const std::string& name) {
    return makeConstant(Constant(name));
}

void BytecodeCompiler::error(const std::string& message) {
    if (!hadError) {         // keep only first error
        hadError = true;
        errorMessage = message;
    }
}

void BytecodeCompiler::errorAt(const std::string& message, int line) {
    if (!hadError) {
        hadError = true;
        errorMessage = "[" + std::to_string(line) + "] " + message;
        throw CompilerError(errorMessage); // Throw to immediately abort current compilation path
    }
}

// ============================================================================
// Loop Handling
// ============================================================================

bool BytecodeCompiler::isConstant(const ExprPtr& expr, Constant& out) {
    if (!expr) return false;
    
    if (auto* litPtr = std::get_if<LiteralExpr*>(&expr->variant)) {
        auto lit = *litPtr;
        if (std::holds_alternative<double>(lit->value)) {
            out = Constant(std::get<double>(lit->value));
            return true;
        } else if (std::holds_alternative<long long>(lit->value)) {
            out = Constant(std::get<long long>(lit->value));
            return true;
        } else if (std::holds_alternative<bool>(lit->value)) {
            out = Constant(std::get<bool>(lit->value));
            return true;
        } else if (std::holds_alternative<std::string>(lit->value)) {
            out = Constant(std::get<std::string>(lit->value));
            return true;
        } else if (std::holds_alternative<std::nullptr_t>(lit->value)) {
            out = Constant(); // NIL
            return true;
        }
    } else if (auto* binPtr = std::get_if<BinaryExpr*>(&expr->variant)) {
        auto bin = *binPtr;
        Constant left, right;
        if (isConstant(bin->left, left) && isConstant(bin->right, right)) {
            if (left.type == Constant::Type::INT && right.type == Constant::Type::INT) {
                long long l = std::get<long long>(left.value);
                long long r = std::get<long long>(right.value);
                // These must produce exactly what the VM would produce for the
                // same expression, or a constant expression silently means
                // something different from its runtime equivalent.
                switch (bin->op) {
                    // Wrapping (defined) arithmetic, matching the VM. Plain
                    // l + r / l - r / l * r here was signed-overflow UB.
                    case TokenType::PLUS:  out = Constant(ezarith::wrapAdd(l, r)); return true;
                    case TokenType::MINUS: out = Constant(ezarith::wrapSub(l, r)); return true;
                    case TokenType::STAR:  out = Constant(ezarith::wrapMul(l, r)); return true;
                    case TokenType::SLASH:
                        // The VM keeps an integer result only when the division is
                        // exact and promotes to double otherwise. This folded with
                        // C++ integer division instead, so `5 / 2` compiled to 2
                        // while `a / b` (a=5, b=2) evaluated to 2.5 at runtime.
                        // Leave the UB cases (÷0, LLONG_MIN / -1) unfolded so the
                        // VM reports them.
                        if (!ezarith::divIsUB(l, r)) {
                            if (ezarith::divIsExact(l, r)) out = Constant(l / r);
                            else out = Constant(static_cast<double>(l) / static_cast<double>(r));
                            return true;
                        }
                        break;
                    default: break;
                }
            } else if (left.type == Constant::Type::DOUBLE || right.type == Constant::Type::DOUBLE) {
                double l = left.type == Constant::Type::INT ? std::get<long long>(left.value) : std::get<double>(left.value);
                double r = right.type == Constant::Type::INT ? std::get<long long>(right.value) : std::get<double>(right.value);
                switch (bin->op) {
                    case TokenType::PLUS: out = Constant(l + r); return true;
                    case TokenType::MINUS: out = Constant(l - r); return true;
                    case TokenType::STAR: out = Constant(l * r); return true;
                    case TokenType::SLASH: if (r != 0.0) { out = Constant(l / r); return true; } break;
                    default: break;
                }
            }
        }
    } else if (auto* unPtr = std::get_if<UnaryExpr*>(&expr->variant)) {
        auto un = *unPtr;
        Constant operand;
        if (isConstant(un->operand, operand)) {
            if (un->op == TokenType::MINUS) {
                if (operand.type == Constant::Type::INT) {
                    // wrapNeg matches the VM's doNegate; plain -x was UB for
                    // LLONG_MIN.
                    out = Constant(ezarith::wrapNeg(std::get<long long>(operand.value)));
                    return true;
                } else if (operand.type == Constant::Type::DOUBLE) {
                    out = Constant(-std::get<double>(operand.value));
                    return true;
                }
            }
        }
    }
    return false;
}



void BytecodeCompiler::startLoop() {
    LoopContext loop;
    loop.start = currentChunk().code.size();
    loopStack.push_back(loop);
}

void BytecodeCompiler::endLoop() {
    loopStack.pop_back();
}

void BytecodeCompiler::emitBreak() {
    if (loopStack.empty()) { errorAt("'break' outside of loop", currentLine); return; }
    // NOTE: do NOT pop a phantom value here; the value stack is balanced
    // at this point by the loop body itself.
    size_t jumpOffset = emitJump(OpCode::JUMP);
    loopStack.back().breaks.push_back(jumpOffset);
}

void BytecodeCompiler::emitContinue() {
    if (loopStack.empty()) { errorAt("'continue' outside of loop", currentLine); return; }
    emitLoop(loopStack.back().start);
}


