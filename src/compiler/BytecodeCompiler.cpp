#include "compiler/BytecodeCompiler.h"
#include <iostream>
#include <memory>
#include <chrono>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
namespace fs = std::filesystem;
#include "lexer/Lexer.h"
#include "parser/Parser.h"

std::unordered_map<std::string, std::string> BytecodeCompiler::virtualFileSystem;

// Helper to get directory of a file




// ============================================================================
// BytecodeCompiler Implementation
// ============================================================================

static int globalCompilerIdCounter = 0;

BytecodeCompiler::Compiler::Compiler(const std::string& name, size_t arity, Compiler* parent)
    : function(std::make_shared<BytecodeFunction>(name, arity)),
      enclosing(parent), scopeDepth(0), currentClass(""), currentParentClass(""),
      maxLocals(0), isHarvesting(false) {
    compilerId = ++globalCompilerIdCounter;
}

BytecodeCompiler::BytecodeCompiler(ASTArena& arena) : arena(arena), current(nullptr), currentLine(0), hadError(false), nextGlobalSlot(0) {}

uint16_t BytecodeCompiler::globalSlotFor(const std::string& name) {
    auto it = globalSlots.find(name);
    if (it != globalSlots.end()) return it->second;
    if (nextGlobalSlot > 65535) {
        errorAt("Too many global variables (max 65535)", currentLine);
        return 0;
    }
    uint16_t slot = nextGlobalSlot++;
    globalSlots[name] = slot;
    return slot;
}

void BytecodeCompiler::setGlobalSlot(const std::string& name, uint16_t slot) {
    globalSlots[name] = slot;
    if (slot >= nextGlobalSlot) {
        nextGlobalSlot = slot + 1;
    }
}

CompileResult BytecodeCompiler::compile(const std::vector<StmtPtr>& statements) {
    CompileResult result;
    hadError = false;
    errorMessage.clear();
    compiledFunctions.clear();

    // Create main function compiler
    std::unique_ptr<Compiler> compilerFrame(new Compiler("<main>", 0, nullptr));
    current = compilerFrame.get();

    // Compile all statements
    try {
        for (const auto& stmt : statements) {
            compileStmt(stmt);
        }
    } catch (const CompilerError&) {
        // Error already recorded in hadError / errorMessage
    }

    // Emit implicit nil return for main
    if (!hadError) {
        emitOp(OpCode::LOAD_NIL);
        emitReturn();
    }

    if (hadError) {
        result.success = false;
        result.error = errorMessage;
        current = nullptr;
        return result;
    }

    result.success = true;
    result.mainFunction = current->function;
    result.mainFunction->localCount = current->maxLocals;
    result.mainFunction->filename = currentFile;  // propagate for stack traces
    result.mainFunction->globalSlotCount = nextGlobalSlot;
    // Export global slot name table for VM initialization
    result.globalSlotNames.resize(nextGlobalSlot);
    for (auto& [name, slot] : globalSlots) {
        result.globalSlotNames[slot] = name;
    }
    // compiledFunctions was populated as inner functions were compiled;
    // add main last so indices assigned during compilation are stable.
    compiledFunctions.push_back(current->function);
    result.functions = compiledFunctions;

    current = nullptr;

    return result;
}

BytecodeFunctionPtr BytecodeCompiler::compileFunction(const TaskStmt& task,
                                                       const std::string& name) {
    // Save enclosing compiler
    Compiler* enclosing = current;

    // Create a new compiler scope for this function
    std::unique_ptr<Compiler> compilerFrame(new Compiler(name, task.params.size(), enclosing));
    current = compilerFrame.get();
    
    // Inherit class context (needed for 'super')
    if (enclosing) {
        current->currentClass = enclosing->currentClass;
        current->currentParentClass = enclosing->currentParentClass;
    }
    // Set default param count (for VM arity check)
    size_t defaultCount = 0;
    for (const auto& dv : task.defaultValues) {
        if (dv != nullptr) defaultCount++;
    }
    current->function->defaultParamCount = defaultCount;
    current->function->isVariadic = task.isVariadic;
    current->function->isAsync = task.isAsync;
    current->function->className = current->currentClass;
    current->isCached = task.isCached;

    if (current->isCached) {
        size_t nameIdx = identifierConstant(name);
        emitOp(OpCode::GET_CACHED_RESULT);
        emitBytes(static_cast<uint8_t>((nameIdx >> 8) & 0xFF),
                  static_cast<uint8_t>(nameIdx & 0xFF));
        
        emitOp(OpCode::DUP);
        emitOp(OpCode::LOAD_NIL);
        emitOp(OpCode::EQUAL);
        size_t hitJump = emitJump(OpCode::JUMP_IF_FALSE);
        
        emitOp(OpCode::POP); // pop nil (miss branch)
        
        size_t skipHit = emitJump(OpCode::JUMP);
        
        patchJump(hitJump);
        emitReturn(); // Return cached value
        
        patchJump(skipHit);
    }

    // Add parameters as the first locals (slot 0, 1, 2, …)
    for (const auto& param : task.params) {
        current->function->paramNames.push_back(param);
        addLocal(param);
        markInitialized();
    }

    // Default parameter initialization prologue:
    // For each parameter that has a default value expression, emit code to
    // check if it is nil and if so, evaluate and assign the default.
    for (size_t i = 0; i < task.defaultValues.size(); ++i) {
        const auto& defaultValue = task.defaultValues[i];
        if (defaultValue != nullptr) {
            // if (param == nil) { param = defaultValue }
            emitLoadLocal(i);
            emitOp(OpCode::LOAD_NIL);
            emitOp(OpCode::EQUAL);
            size_t jump = emitJump(OpCode::JUMP_IF_FALSE);

            compileExpr(defaultValue);
            emitStoreLocal(i);
            emitOp(OpCode::POP); // pop result of STORE_LOCAL (peek design)

            patchJump(jump);
        }
    }

    // ---- @ratelimit check ----
    if (task.rateLimit != nullptr) {
        const auto& rl = *task.rateLimit;
        // Push: key (string "global"), count, per
        size_t globalKey = identifierConstant("global");
        emitOp(OpCode::LOAD_CONST);
        emitBytes(static_cast<uint8_t>((globalKey >> 8) & 0xFF), static_cast<uint8_t>(globalKey & 0xFF));
        // Push count as number
        double countVal = static_cast<double>(rl.count);
        int constIdx = (int)makeConstant(Constant(countVal));
        emitOp(OpCode::LOAD_CONST);
        emitBytes(static_cast<uint8_t>((constIdx >> 8) & 0xFF), static_cast<uint8_t>(constIdx & 0xFF));
        // Push per-string
        size_t perIdx = identifierConstant(rl.per);
        emitOp(OpCode::LOAD_CONST);
        emitBytes(static_cast<uint8_t>((perIdx >> 8) & 0xFF), static_cast<uint8_t>(perIdx & 0xFF));
        // Emit RATELIMIT_CHECK with task name as key
        size_t nameIdx = identifierConstant(task.name);
        emitOp(OpCode::RATELIMIT_CHECK);
        emitBytes(static_cast<uint8_t>((nameIdx >> 8) & 0xFF), static_cast<uint8_t>(nameIdx & 0xFF));
    }

    // ---- Design-by-Contract: requires (preconditions) ----
    // Emitted AFTER default params are set, so they can reference params by name.
    if (!disableContracts && !task.requiresClauses.empty()) {
        compileContractChecks(task.requiresClauses, true);
    }
    
    // ---- Design-by-Contract: old() capture for ensures ----
    // Scan ensures clauses for old(expr) calls and capture values into hidden locals.
    oldCaptures.clear();
    if (!disableContracts && !task.ensuresClauses.empty()) {
        // We do a simple text scan of the ensures expressions looking for CallExpr
        // with callee name "old". We capture each unique old(expr) into a hidden local.
        std::function<void(const ExprPtr&)> captureOldExprs = [&](const ExprPtr& e) {
            if (!e) return;
            if (auto* call = std::get_if<CallExpr*>(&e->variant)) {
                auto& c = **call;
                // Check if callee is identifier "old"
                if (auto* id = std::get_if<IdentifierExpr*>(&c.callee->variant)) {
                    if ((*id)->name == "old" && c.arguments.size() == 1) {
                        // Build a stable key from source position of the argument
                        std::string key = "old_" + std::to_string(c.arguments[0]->line) + "_" +
                                                   std::to_string(c.arguments[0]->column);
                        if (oldCaptures.find(key) == oldCaptures.end()) {
                            // Evaluate the inner expression NOW and store to a hidden local
                            compileExpr(c.arguments[0]);
                            size_t slot = addLocal("__old_" + key + "__");
                            markInitialized();
                            emitStoreLocal(slot);
                            emitOp(OpCode::POP);
                            oldCaptures[key] = slot;
                        }
                        return; // Don't recurse into old's argument again
                    }
                }
                // Recurse into call arguments
                captureOldExprs(c.callee);
                for (auto& arg : c.arguments) captureOldExprs(arg);
            } else {
                // Generic recursive visit through other expr types
                std::visit([&](auto&& node) {
                    using T = std::decay_t<decltype(node)>;
                    if constexpr (std::is_same_v<T, BinaryExpr*>) {
                        captureOldExprs(node->left); captureOldExprs(node->right);
                    } else if constexpr (std::is_same_v<T, UnaryExpr*>) {
                        captureOldExprs(node->operand);
                    } else if constexpr (std::is_same_v<T, LogicalExpr*>) {
                        captureOldExprs(node->left); captureOldExprs(node->right);
                    } else if constexpr (std::is_same_v<T, TernaryExpr*>) {
                        captureOldExprs(node->condition); captureOldExprs(node->thenBranch); captureOldExprs(node->elseBranch);
                    }
                }, e->variant);
            }
        };
        for (auto& [cond, _msg] : task.ensuresClauses) {
            captureOldExprs(cond);
        }
    }

    // Thread ensures clauses so compileGive can insert postconditions.
    auto* savedEnsures = currentEnsuresClauses;
    auto savedOldCaptures = oldCaptures;
    currentEnsuresClauses = task.ensuresClauses.empty() ? nullptr : &task.ensuresClauses;

    beginScope();
    for (const auto& stmt : task.body) {
        compileStmt(stmt);
    }
    endScope();

    // Restore ensures context for enclosing function
    currentEnsuresClauses = savedEnsures;
    oldCaptures = savedOldCaptures;

    // Implicit nil return (no ensures checks — bare fall-through is nil result)
    emitOp(OpCode::LOAD_NIL);
    if (current->isCached) {
        size_t nameIdx = identifierConstant(name);
        emitOp(OpCode::STORE_CACHED_RESULT);
        emitBytes(static_cast<uint8_t>((nameIdx >> 8) & 0xFF),
                  static_cast<uint8_t>(nameIdx & 0xFF));
    }
    emitReturn();

    BytecodeFunctionPtr result = current->function;
    result->localCount = current->maxLocals;
    // Propagate the source filename so stack traces can show which file this function is from
    result->filename = currentFile;

    // Register this function in compiledFunctions so the VM can find it.
    // The index assigned here is what CLOSURE will use.
    compiledFunctions.push_back(result);

    current = enclosing;

    return result;
}

// ============================================================================
// Expression Compilation
// ============================================================================

