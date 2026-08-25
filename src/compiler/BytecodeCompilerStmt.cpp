#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
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
        emitStoreLocal(localIdx);
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
    emitStoreLocal(loopVar);

    // End value cached in a hidden local (slot N+1)
    size_t endVar = addLocal("<repeat-end>");
    compileExpr(stmt.end);
    markInitialized();
    emitStoreLocal(endVar);

    if (!stmt.step) {
        startLoop();

        // The increment is placed at the TOP of the loop, jumped over on first
        // entry, so that it sits exactly where `skip` lands.
        //
        // Laid out the obvious way -- condition, body, increment, jump back --
        // the loop's start is the condition, and `skip` (which jumps to the
        // loop start) lands there having stepped straight over the increment.
        // The counter then never advances and `repeat i = 1 to 5 { skip }`
        // spins forever. `while` and `get..in` are unaffected: for them the
        // start IS the right place to resume.
        //
        //        JUMP -> bodyEntry     (skip the increment on the first pass)
        //   incPoint:                  <- loop start, and where `skip` lands
        //        INC_LOCAL
        //   bodyEntry:
        //        condition / exit
        //        <body>
        //        LOOP -> incPoint
        size_t skipFirstInc = emitJump(OpCode::JUMP);

        size_t incPoint = currentChunk().code.size();
        loopStack.back().start = incPoint;

        // INC_LOCAL and LOOP_LESS_EQ_LOCAL are the fused fast path, and both
        // carry single-byte slots. Past slot 255 they would silently truncate
        // and drive the wrong local, so a loop that far into a large function
        // falls back to the generic sequence through the wide-aware helpers.
        const bool narrowSlots = (loopVar <= 0xFF && endVar <= 0xFF);

        size_t exitJump = 0;
        if (narrowSlots) {
            emitBytes(static_cast<uint8_t>(OpCode::INC_LOCAL), static_cast<uint8_t>(loopVar));
            patchJump(skipFirstInc);

            emitOp(OpCode::LOOP_LESS_EQ_LOCAL);
            emitByte(static_cast<uint8_t>(loopVar));
            emitByte(static_cast<uint8_t>(endVar));
            emitByte(0xFF); emitByte(0xFF); emitByte(0xFF); emitByte(0xFF);
            exitJump = currentChunk().code.size() - 4;
        } else {
            // loopVar = loopVar + 1
            emitLoadLocal(loopVar);
            emitOp(OpCode::LOAD_ONE);
            emitOp(OpCode::ADD);
            emitStoreLocal(loopVar);
            emitOp(OpCode::POP);

            patchJump(skipFirstInc);

            // while endVar >= loopVar
            emitLoadLocal(endVar);
            emitLoadLocal(loopVar);
            emitOp(OpCode::GREATER_EQ);
            exitJump = emitJump(OpCode::JUMP_IF_FALSE);
        }

        compileStmt(stmt.body);

        emitLoop(incPoint);
        patchJump(exitJump);
        for (size_t breakOffset : loopStack.back().breaks) patchJump(breakOffset);
        endLoop();
    } else {
        // Step value cached in a hidden local (slot N+2)
        size_t stepVar = addLocal("<repeat-step>");
        compileExpr(stmt.step);
        markInitialized();
        emitStoreLocal(stepVar);
        
        // --- Runtime check: step != 0 ---
        emitLoadLocal(stepVar);
        emitOp(OpCode::LOAD_ZERO);
        emitOp(OpCode::EQUAL);
        size_t stepZeroJump = emitJump(OpCode::JUMP_IF_FALSE);
        
        int errStrIdx = (int)makeConstant(Constant("Step cannot be 0 in repeat loop"));
        emitOp(OpCode::LOAD_CONST);
        emitBytes(static_cast<uint8_t>((errStrIdx >> 8) & 0xFF), static_cast<uint8_t>(errStrIdx & 0xFF));
        emitOp(OpCode::THROW);

        patchJump(stepZeroJump);

        startLoop();

        // Same shape as the unstepped loop above: the `loopVar += step` update
        // goes at the top, jumped over on first entry, so that `skip` lands on
        // it rather than stepping over it and spinning forever.
        size_t skipFirstStep = emitJump(OpCode::JUMP);

        size_t incPoint = currentChunk().code.size();
        loopStack.back().start = incPoint;
        emitLoadLocal(loopVar);
        emitLoadLocal(stepVar);
        emitOp(OpCode::ADD);
        emitStoreLocal(loopVar);
        emitOp(OpCode::POP);

        patchJump(skipFirstStep);

        // Condition: (step > 0) ? (loopVar <= endVar) : (loopVar >= endVar)
        emitLoadLocal(stepVar);
        emitOp(OpCode::LOAD_ZERO);
        emitOp(OpCode::GREATER);
        size_t stepJump = emitJump(OpCode::JUMP_IF_FALSE);

        // Positive step: endVar >= loopVar
        emitLoadLocal(endVar);
        emitLoadLocal(loopVar);
        emitOp(OpCode::GREATER_EQ); 
        size_t endStepJump = emitJump(OpCode::JUMP);

        patchJump(stepJump);

        // Negative step: loopVar >= endVar
        emitLoadLocal(loopVar);
        emitLoadLocal(endVar);
        emitOp(OpCode::GREATER_EQ); 

        patchJump(endStepJump);
        
        size_t exitJump = emitJump(OpCode::JUMP_IF_FALSE);

        compileStmt(stmt.body);

        // The update now lives at incPoint, above.
        emitLoop(incPoint);
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
    emitStoreLocal(iterVar);
    emitOp(OpCode::POP);

    // Loop variable that receives each element (or key)
    size_t loopVar = addLocal(stmt.variable);
    current->locals.back().isStackResident = false;
    markInitialized();
    emitOp(OpCode::LOAD_NIL);
    emitStoreLocal(loopVar);
    emitOp(OpCode::POP);
    
    // Optional second variable for value in dict iteration
    size_t loopValueVar = 0;
    if (!stmt.valueVariable.empty()) {
        loopValueVar = addLocal(stmt.valueVariable);
        current->locals.back().isStackResident = false;
        markInitialized();
        emitOp(OpCode::LOAD_NIL);
        emitStoreLocal(loopValueVar);
        emitOp(OpCode::POP);
    }

    startLoop();
    size_t loopStart = currentChunk().code.size();
    loopStack.back().start = loopStart;

    emitLoadLocal(iterVar);
    size_t exitJump = emitJump(OpCode::ITER_NEXT);

    if (!stmt.valueVariable.empty()) {
        // We have [key, value] on the stack. Destructure it.
        emitOp(OpCode::DUP); // [array, array]
        
        // key = array[0]
        emitOp(OpCode::LOAD_ZERO);
        emitOp(OpCode::INDEX_GET); // [array, key]
        emitStoreLocal(loopVar);
        emitOp(OpCode::POP); // [array]
        
        // value = array[1]
        emitOp(OpCode::LOAD_ONE);
        emitOp(OpCode::INDEX_GET); // [value]
        emitStoreLocal(loopValueVar);
        emitOp(OpCode::POP); // []
    } else {
        // Store the value that ITER_NEXT left on the stack into loopVar
        emitStoreLocal(loopVar);
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
        // 16-bit capture index. This is the slot of the captured variable in
        // the ENCLOSING function, so once a function can hold more than 256
        // locals the old single byte silently wrapped: capturing local 300
        // captured local 44 instead, and a closure quietly bound to the wrong
        // variable -- in practice a task that called itself instead of the
        // function it meant to.
        emitBytes(static_cast<uint8_t>((uv.index >> 8) & 0xFF),
                  static_cast<uint8_t>(uv.index & 0xFF));
    }
}

void BytecodeCompiler::compileTask(const TaskStmt& stmt) {
    // 1. Load the decorators, OUTERMOST first, so the innermost ends up nearest
    //    the closure on the stack and is therefore applied first.
    //
    //    The loop used to run in reverse, which inverted the whole thing: for
    //
    //        @app.get("/dash")
    //        @auth.login_required
    //        task dash(req) { ... }
    //
    //    it produced login_required(app.get(...)(dash)) -- app.get ran FIRST and
    //    registered the undecorated handler, so the auth guard was applied to a
    //    function nothing would ever call and the route was silently public.
    //    Every language with decorators applies them bottom-up; so do we now.
    for (const auto& dec : stmt.userDecorators) {
        compileExpr(dec);
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
        emitStoreLocal(local);
    } else if (current->scopeDepth > 0) {
        // In namespaced modules (depth 1) or nested functions, tasks are locals
        size_t slot = addLocal(stmt.name);
        markInitialized();
        emitStoreLocal(slot);
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
    // A pending finally rules out TCO: a tail call reuses the frame and jumps
    // away, so the finally body emitted below would never run.
    bool isTailCall = false;
    if (stmt.value && current && !current->isCached
        && current->activeFinallys.empty()
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

                    // Matching the callee by NAME is only a guess: the name may
                    // resolve to something other than this function (a global
                    // builtin it shadows, or a reassigned variable). The VM
                    // verifies the callee's identity and only reuses the frame
                    // when it really is a self-call; otherwise it performs an
                    // ordinary call and falls through to here.
                    //
                    // So emit the RETURN that ordinary call needs. Previously
                    // nothing was emitted ("TAIL_CALL handles its own return"),
                    // and the non-self path fell into the function's implicit
                    // trailing LOAD_NIL + RETURN -- silently discarding the
                    // result and returning nil. That is why a task that
                    // tail-called a builtin it shadowed, e.g.
                    //     static task urlDecode(s) { give urlDecode(s) }
                    // returned nil for every input.
                    //
                    // On the self-tail-call path the VM jumps back to the
                    // function's first instruction, so this RETURN is never
                    // reached and frame reuse is unaffected.
                    emitOp(OpCode::RETURN);
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
            emitStoreLocal(resultSlot);
            emitOp(OpCode::POP);

            size_t resultAliasSlot = addLocal("result");
            markInitialized();
            emitLoadLocal(resultSlot);
            emitStoreLocal(resultAliasSlot);
            emitOp(OpCode::POP);

            compileContractChecks(*currentEnsuresClauses, false);

            emitLoadLocal(resultSlot);
        }

        // Replay any open finally blocks before actually leaving.
        //
        // The finally body is emitted INLINE here, in addition to the copy on the
        // normal fall-through path. Without this a `give` jumped straight to
        // RETURN and the finally never ran at all:
        //
        //     try { give x } finally { cleanup() }      # cleanup() skipped
        //     try { ... } catch (e) { give y } finally { ... }   # also skipped
        //
        // Only a try/catch that fell off its own end ever ran one, which made
        // `finally` useless for exactly the job it exists for -- releasing a
        // handle on every exit path.
        //
        // Innermost first: `try { try { give 1 } finally { A } } finally { B }`
        // must run A then B. Each block parks the return value in its own hidden
        // local across its body, so the finally's own locals sit at the slot
        // offsets the compiler assigned them.
        //
        // While block i's body is compiled, blocks i..end are taken OFF the
        // active list. A `give` inside a finally must not replay the block it is
        // standing in -- that recursed forever right here in the compiler and
        // killed the process with no output at all:
        //
        //     try { give "a" } finally { give "b" }
        //
        // Dropping them is also the correct semantics: such a `give` should still
        // run the finallys OUTSIDE it, and those are exactly what remains.
        auto pending = current->activeFinallys;
        for (size_t i = pending.size(); i-- > 0; ) {
            current->activeFinallys.resize(i);
            emitStoreLocal(pending[i].retvalSlot);
            emitOp(OpCode::POP);
            compileStmt(pending[i].body);
            emitLoadLocal(pending[i].retvalSlot);
        }
        current->activeFinallys = pending;

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
    // If it's an export of an existing variable (e.g. `export alloc`),
    // find the local variable by name and mark it as exported.
    if (std::holds_alternative<ExpressionStmt*>(stmt.inner->variant)) {
        auto exprStmt = std::get<ExpressionStmt*>(stmt.inner->variant);
        if (std::holds_alternative<IdentifierExpr*>(exprStmt->expr->variant)) {
            auto idExpr = std::get<IdentifierExpr*>(exprStmt->expr->variant);
            int slot = resolveLocal(idExpr->name);
            if (slot != -1) {
                current->locals[slot].exported = true;
                return; // We don't need to evaluate and pop it
            }
        }
    }

    // Compile the inner declaration normally
    size_t localsBefore = current->locals.size();

    // A declaration does not always introduce a NEW local. Top-level tasks in a
    // module are pre-declared so they can call each other regardless of the
    // order they appear in, so `export task f()` reuses that existing slot and
    // the range check below would mark nothing -- leaving f unexported and nil
    // to anyone importing the package. Remember the declared name and mark it
    // explicitly as well.
    std::string declName;
    if (std::holds_alternative<TaskStmt*>(stmt.inner->variant)) {
        declName = std::get<TaskStmt*>(stmt.inner->variant)->name;
    } else if (std::holds_alternative<ModelStmt*>(stmt.inner->variant)) {
        declName = std::get<ModelStmt*>(stmt.inner->variant)->name;
    }

    compileStmt(stmt.inner);
    // Mark all newly introduced locals as exported
    for (size_t i = localsBefore; i < current->locals.size(); i++) {
        current->locals[i].exported = true;
    }
    if (!declName.empty()) {
        int slot = resolveLocal(declName);
        if (slot != -1) current->locals[slot].exported = true;
    }
}

// ── Module resolution failures ───────────────────────────────────────────────
//
// "Could not find module 'htlm'" is true and useless. The three things the
// reader needs are which name failed, where the compiler looked, and whether
// something close is installed -- because the answer is a typo far more often
// than a missing package.

// Edit distance, capped: anything past `limit` is not a suggestion worth
// making, and stopping early keeps this linear in practice.
static size_t nameDistance(const std::string& a, const std::string& b, size_t limit) {
    if (a == b) return 0;
    if (a.size() > b.size() + limit || b.size() > a.size() + limit) return limit + 1;

    std::vector<size_t> previous(b.size() + 1), current(b.size() + 1);
    for (size_t j = 0; j <= b.size(); ++j) previous[j] = j;
    for (size_t i = 1; i <= a.size(); ++i) {
        current[0] = i;
        size_t best = current[0];
        for (size_t j = 1; j <= b.size(); ++j) {
            size_t cost = (std::tolower((unsigned char)a[i - 1]) ==
                           std::tolower((unsigned char)b[j - 1])) ? 0 : 1;
            current[j] = std::min({ previous[j] + 1, current[j - 1] + 1, previous[j - 1] + cost });
            best = std::min(best, current[j]);
        }
        if (best > limit) return limit + 1;   // no cell in this row can recover
        previous = current;
    }
    return previous[b.size()];
}

// Installed package names that are close to what was asked for.
// Only scans <exe_dir>/lib/ — the single canonical library root.
static std::vector<std::string> similarModules(const std::string& wanted) {
    std::vector<std::pair<size_t, std::string>> scored;
    // Two edits for a name of any length, one for a short name where two edits
    // would match almost anything.
    size_t limit = wanted.size() <= 4 ? 1 : 2;

    std::error_code ec;
    fs::directory_iterator it(ezLibBase(), ec), end;
    if (!ec) {
        for (; it != end; it.increment(ec)) {
            if (ec) { ec.clear(); break; }
            std::string name = it->path().filename().string();
            if (!name.empty() && name[0] == '.') continue;
            if (name.size() > 3 && name.substr(name.size() - 3) == ".ez")
                name = name.substr(0, name.size() - 3);
            if (name == wanted) continue;
            size_t d = nameDistance(wanted, name, limit);
            if (d <= limit) scored.push_back({ d, name });
        }
    }
    std::sort(scored.begin(), scored.end());
    std::vector<std::string> names;
    for (const auto& entry : scored) {
        if (std::find(names.begin(), names.end(), entry.second) == names.end())
            names.push_back(entry.second);
        if (names.size() == 3) break;
    }
    return names;
}

std::string BytecodeCompiler::moduleNotFound(const std::string& path,
                                             const std::vector<std::string>& searched,
                                             const std::string& directoryHit) {
    std::string out = "ModuleNotFoundError: no module named '" + path + "'";

    if (!directoryHit.empty()) {
        // The package directory exists but has no entry point -- a different
        // problem with a different fix, so say so rather than claiming the
        // module does not exist.
        out = "ModuleNotFoundError: '" + path + "' has no entry point\n\n"
              "  Found the directory:\n    " + directoryHit + "\n"
              "  but neither " + path + ".ez nor main.ez is inside it.\n\n"
              "  A package directory needs one of those, and package.ez should\n"
              "  name it in \"main\".";
        return out;
    }

    out += "\n\n  Searched:\n";
    std::vector<std::string> seen;
    for (const std::string& candidate : searched) {
        if (std::find(seen.begin(), seen.end(), candidate) != seen.end()) continue;
        seen.push_back(candidate);
        out += "    " + candidate + "\n";
    }

    std::vector<std::string> similar = similarModules(path);
    if (!similar.empty()) {
        out += "\n  Did you mean ";
        for (size_t i = 0; i < similar.size(); ++i) {
            if (i) out += (i + 1 == similar.size()) ? " or " : ", ";
            out += "'" + similar[i] + "'";
        }
        out += "?";
    } else {
        out += "\n  If it is a published package:  ez install " + path;
    }

    // A local file must be imported with its extension: a bare relative path
    // is resolved under package semantics and would hand back an empty
    // namespace, so the compiler deliberately does not guess.
    if (path.find(".ez") == std::string::npos && path.find('/') != std::string::npos) {
        out += "\n  For a file in your project, include the extension:  use \"" + path + ".ez\"";
    }
    out += "\n  Library root: " + ezLibBase();
    return out;
}

// ── Python-style module resolution ───────────────────────────────────────────
//
// Resolution order (like Python's sys.path):
//   1. Relative to the importing file   (for use "sub/file.ez")
//   2. Virtual File System               (bundled executables only)
//   3. <exe_dir>/lib/                    (THE canonical library root)
//
// Within step 3, for a bare name like `use "http"`:
//   a) lib/<name>.ez               — single-file module
//   b) lib/<name>/                 — directory package:
//        i)   read package.ez → "main" field → that file
//        ii)  <name>/<name>.ez
//        iii) <name>/main.ez

// Helper: try to read "main" from a package.ez manifest inside a directory.
static std::string resolvePackageEntry(const std::string& dirPath) {
    std::string pkgFile = dirPath + "/package.ez";
    std::ifstream pf(pkgFile);
    if (!pf.is_open()) return "";
    std::stringstream buf;
    buf << pf.rdbuf();
    std::string content = buf.str();
    // Minimal JSON parse: find "main": "someFile.ez"
    size_t mainKey = content.find("\"main\"");
    if (mainKey == std::string::npos) return "";
    size_t colon = content.find(':', mainKey + 6);
    if (colon == std::string::npos) return "";
    size_t qStart = content.find('"', colon + 1);
    if (qStart == std::string::npos) return "";
    size_t qEnd = content.find('"', qStart + 1);
    if (qEnd == std::string::npos) return "";
    return content.substr(qStart + 1, qEnd - qStart - 1);
}

void BytecodeCompiler::compileUse(const UseStmt& stmt) {
    std::string path = stmt.path;
    std::string absolutePath = path;

    // Every location tried, in order, for diagnostics.
    std::vector<std::string> searched;

    // ── Step 0: Resolve relative to current file ──
    if (!currentFile.empty() && currentFile != "repl" && currentFile != "main" && currentFile != "<string>" && currentFile != "-c") {
        std::string dir = getDirectoryName(currentFile);
        if (dir != ".") {
            absolutePath = dir + "/" + path;
        }
    }

    std::string source;
    std::string ezlibBase = ezLibBase();

    // ── Step 1: Virtual File System (bundled executables) ──
    bool foundInVFS = false;

    // VFS keys to try, in priority order
    std::vector<std::string> vfsKeys = {
        path,
        path + ".ez",
        "lib/" + path + ".ez",
        "lib/" + path + "/" + path + ".ez",
        "lib/" + path + "/main.ez",
        ezlibBase + path,
        ezlibBase + path + ".ez",
        ezlibBase + path + "/" + path + ".ez",
        ezlibBase + path + "/main.ez"
    };
    for (const auto& key : vfsKeys) {
        if (virtualFileSystem.count(key)) {
            source = virtualFileSystem[key];
            absolutePath = key;
            foundInVFS = true;
            break;
        }
    }

    // ── Step 2: Filesystem resolution ──
    if (!foundInVFS) {
        // Helper: try opening a candidate path and claim it if it works.
        bool found = false;
        auto tryPath = [&](const std::string& candidate) -> bool {
            searched.push_back(candidate);
            std::ifstream f(candidate);
            if (f.is_open()) {
                absolutePath = candidate;
                std::stringstream buf;
                buf << f.rdbuf();
                source = buf.str();
                found = true;
                return true;
            }
            return false;
        };

        // 2a. Relative to importing file (absolutePath was set in step 0)
        if (absolutePath != path) {
            tryPath(absolutePath);
        }

        // 2b. Exact path as typed (for absolute paths or use "file.ez")
        if (!found) {
            tryPath(path);
        }

        // 2c. <exe_dir>/lib/ — the single canonical library root
        if (!found) {
            std::string libPath = ezlibBase + path;

            // Try exact match in lib (e.g. lib/somefile)
            if (!found) tryPath(libPath);

            // Try with .ez extension (e.g. lib/http.ez)
            if (!found && path.find(".ez") == std::string::npos) {
                tryPath(libPath + ".ez");
            }

            // Try as a directory package
            if (!found && fs::is_directory(libPath)) {
                // i) Check package.ez for "main" field
                std::string pkgEntry = resolvePackageEntry(libPath);
                if (!pkgEntry.empty()) {
                    tryPath(libPath + "/" + pkgEntry);
                }

                // ii) <name>/<name>.ez
                if (!found) {
                    tryPath(libPath + "/" + path + ".ez");
                }

                // iii) <name>/main.ez
                if (!found) {
                    tryPath(libPath + "/main.ez");
                }

                // Directory exists but no entry point found
                if (!found) {
                    errorAt(moduleNotFound(path, searched, libPath), currentLine);
                    return;
                }
            }

            if (!found) {
                errorAt(moduleNotFound(path, searched, ""), currentLine);
                return;
            }
        }
    }
    
    if (compilingModules.count(absolutePath)) {
        errorAt(std::string("CircularImportError: '") + absolutePath +
                "' is already being imported\n"
                "\n"
                "  A module cannot import something that is (directly or indirectly)\n"
                "  importing it. Move the shared code both need into a third module\n"
                "  that each of them imports.", currentLine);
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
            errorAt("SyntaxError: could not tokenize module '" + absolutePath + "'\n"
                    "\n  The module reported the offending line above; this is the"
                    "\n  `use` that pulled it in.", currentLine);
            return;
        }
        
        Parser parser(tokens, arena);
        statements = parser.parse();
        if (parser.hasError()) {
            errorAt("SyntaxError: could not parse module '" + absolutePath + "'\n"
                    "\n  The module reported the offending line above; this is the"
                    "\n  `use` that pulled it in.", currentLine);
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

    // Namespaced AND Global imports both compile as a module closure to enable caching
    std::unique_ptr<Compiler> moduleCompiler(new Compiler(alias + "_module", 0, current));
    Compiler* previous = current;
    current = moduleCompiler.get();

    // Force module top-level to be locals by starting at depth 1
    current->scopeDepth = 1;
    current->isHarvesting = true;
    
    // Reserve slot 0 for the module closure itself (standard VM frame layout)
    addLocal("");
    markInitialized();

    // Declare every top-level task before compiling any body, so that one can
    // call another regardless of the order they appear in.
    //
    // Without this, a reference to a task declared further down the file found
    // no local and fell back to a GLOBAL read, while the declaration itself
    // stored a module LOCAL (scopeDepth is forced to 1 above). The two never
    // met, and the call produced "Value is not callable: nil".
    //
    // It only showed up in a module imported BY another module. Imported at top
    // level the module's symbols are unpacked into globals, so the global read
    // happened to find one; nested a level deeper they are unpacked into the
    // importer's locals and the global stayed empty. sqlite hit this in
    // migrate.ez, where migrate() calls _sort_by_version() declared below it.
    //
    // Pre-declaring also makes mutual recursion between top-level tasks work.
    for (const auto& s : statements) {
        if (!s) continue;
        const Stmt* target = s;
        if (std::holds_alternative<ExportStmt*>(target->variant)) {
            target = std::get<ExportStmt*>(target->variant)->inner;
            if (!target) continue;
        }
        if (std::holds_alternative<TaskStmt*>(target->variant)) {
            const std::string& tname = std::get<TaskStmt*>(target->variant)->name;
            if (!tname.empty() && resolveLocal(tname) == -1) {
                addLocal(tname);
                markInitialized();
            }
        }
    }

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
                
                // Find where the local is (using resolveLocal to be safe).
                // This is the module-export harvest, which is precisely where a
                // slot past 255 shows up, so it must use the wide-aware helper.
                int slot = resolveLocal(local.name);
                emitLoadLocal(slot);
                
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
            // 16-bit: uv.index is a slot in the enclosing function, which can
            // hold more than 256 locals. See the CLOSURE emit above.
            emitBytes(static_cast<uint8_t>((uv.index >> 8) & 0xFF),
                      static_cast<uint8_t>(uv.index & 0xFF));
        }

        emitOp(OpCode::CALL);
        emitByte(0); // 0 args

        // Store to global cache
        emitOp(OpCode::STORE_GLOBAL);
        emitBytes(static_cast<uint8_t>((flagIdx >> 8) & 0xFF), static_cast<uint8_t>(flagIdx & 0xFF));
        emitOp(OpCode::POP);
        
        patchJump(skipExec);
        
        // Load from global cache (which now definitely holds the module dictionary)
        emitOp(OpCode::LOAD_GLOBAL);
        emitBytes(static_cast<uint8_t>((flagIdx >> 8) & 0xFF), static_cast<uint8_t>(flagIdx & 0xFF));

        if (alias == "*") {
            // Unpack all exported locals into the current scope
            for (const auto& local : moduleCompiler->locals) {
                if (local.depth == 1 && !local.name.empty()) {
                    if (!isFileInclusion && !local.exported) continue;
                    
                    // DUP dictionary
                    emitOp(OpCode::DUP);
                    
                    // LOAD_PROPERTY
                    emitOp(OpCode::LOAD_PROPERTY);
                    size_t propIdx = identifierConstant(local.name);
                    emitBytes(static_cast<uint8_t>((propIdx >> 8) & 0xFF),
                              static_cast<uint8_t>(propIdx & 0xFF));
                    size_t icIdx = current->function->chunk.icEntries.size();
                    current->function->chunk.icEntries.push_back(ICCacheEntry{});
                    emitBytes(static_cast<uint8_t>((icIdx >> 8) & 0xFF), static_cast<uint8_t>(icIdx & 0xFF));
                    
                    // Store in current scope, reusing the slot when this name is
                    // already bound here.
                    //
                    // Imports form a diamond as soon as a package has more than
                    // a couple of files: sqlite's connection.ez pulls in ffi.ez
                    // directly AND again through statement.ez, so allocating a
                    // fresh slot per occurrence brought ffi's 68 symbols in
                    // twice over and blew the 256-local ceiling. Reusing the
                    // slot also keeps last-import-wins, which is what a second
                    // binding of the same name did before.
                    if (current->scopeDepth > 0) {
                        int existing = resolveLocal(local.name);
                        size_t slot;
                        if (existing != -1) {
                            // Already bound and already initialised. Not calling
                            // markInitialized() here is deliberate: it marks
                            // locals.back(), so on the reuse path it would set
                            // the depth of whatever was declared most recently
                            // rather than the slot being written.
                            slot = static_cast<size_t>(existing);
                        } else {
                            slot = addLocal(local.name);
                            markInitialized();
                        }
                        // Carry the export flag across the boundary.
                        //
                        // Without this a symbol stopped propagating after one
                        // hop: `use "src/ffi.ez"` brought make_sockaddr into
                        // socket/main.ez as an ordinary (unexported) local, so
                        // `use "socket"` -- which keeps only exported locals --
                        // filtered it straight back out and callers saw nil.
                        // Any package built as a thin main.ez over a src/
                        // directory was therefore impossible: sqlite, socket and
                        // orm are all laid out exactly that way.
                        //
                        // Only what the inner module explicitly marked `export`
                        // travels onward, so a file inclusion's private helpers
                        // still stay private.
                        current->locals[slot].exported = local.exported;
                        emitStoreLocal(slot);
                        emitOp(OpCode::POP);
                    } else {
                        uint16_t aliasSlot = globalSlotFor(local.name);
                        emitOp(OpCode::STORE_GLOBAL_SLOT);
                        emitBytes(static_cast<uint8_t>((aliasSlot >> 8) & 0xFF),
                                  static_cast<uint8_t>(aliasSlot & 0xFF));
                        emitOp(OpCode::POP);
                    }
                }
            }
            emitOp(OpCode::POP); // Pop the dictionary itself
        } else {
            // Store into the requested alias
            if (current->scopeDepth > 0) {
                size_t slot = addLocal(alias);
                markInitialized();
                emitStoreLocal(slot);
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
    // 0. Outermost first, so the innermost decorator is applied first -- same
    //    ordering fix as compileTask(); see the note there.
    for (const auto& dec : stmt.userDecorators) {
        compileExpr(dec);
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
            TaskStmt methodTask(member.name, params, std::vector<TypeASTPtr>(params.size(), arena.allocate<TypeAST>("Any")), defaults, nullptr, member.body, member.isVariadic, member.isAsync);
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
        emitStoreLocal(slot);
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
    //
    // Stored BY NAME, not by slot. Globals live in two places -- a name-keyed
    // map and a slot array -- and a slot store is invisible to a name lookup.
    // This wrote the slot while HAS_GLOBAL (above) and LOAD_GLOBAL (the read in
    // compileIdentifier) both consult the name map, so the value never landed
    // anywhere either of them could see it:
    //
    //     task inc() { static x = 0  x += 1  give x }
    //     inc()   ->  Undefined variable '__static_2_inc_x'
    //
    // The name is also what makes the once-only guard work: had only the store
    // been switched to a slot, HAS_GLOBAL would have stayed false forever and
    // the initializer would have re-run on every call, resetting the static.
    compileExpr(stmt.initializer);
    emitOp(OpCode::STORE_GLOBAL);
    emitBytes(static_cast<uint8_t>((nameIdx >> 8) & 0xFF),
              static_cast<uint8_t>(nameIdx & 0xFF));
    emitOp(OpCode::POP);
    
    // 4. Patch jump
    patchJump(skipInit);
}

void BytecodeCompiler::compileTry(const TryStmt& stmt) {
    bool hasFinally = stmt.finallyBlock != nullptr;
    bool hasCatch = !stmt.catchBlocks.empty();
    
    int pendingSlot = -1;
    int retvalSlot = -1;
    if (hasFinally) {
        beginScope(); // Scope for the hidden pending exception variable
        emitOp(OpCode::LOAD_NIL);
        pendingSlot = addLocal("__pendingExc__" + std::to_string(current->locals.size()));
        current->locals.back().isStackResident = true; // It is on the stack right now
        markInitialized();

        // A second hidden local, to park a `give`'s return value while the
        // finally body runs. The value cannot simply be left on the stack: the
        // finally block opens a scope whose locals are addressed by slot index,
        // and an extra temporary underneath them would shift every one.
        emitOp(OpCode::LOAD_NIL);
        retvalSlot = addLocal("__retval__" + std::to_string(current->locals.size()));
        current->locals.back().isStackResident = true;
        markInitialized();
    }

    // Register the finally for the try/catch bodies compiled below, so a `give`
    // inside them replays it before returning. Deliberately NOT registered while
    // the finally block itself is compiled further down -- a `give` inside a
    // finally must not re-enter it.
    if (hasFinally) {
        current->activeFinallys.push_back(Compiler::ActiveFinally{stmt.finallyBlock, retvalSlot});
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
                    emitStoreLocal(slot);
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
                    emitStoreLocal(slot);
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
            emitStoreLocal(pendingSlot);
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
        emitStoreLocal(pendingSlot);
        emitOp(OpCode::POP);
    }

    // Patch all successful catch handlers and afterCatch to jump here
    for (size_t jump : successJumps) {
        patchJump(jump);
    }
    patchJump(afterCatch);

    // Emit finally block if present
    if (hasFinally) {
        // Out of scope from here on: this is the normal fall-through copy of the
        // finally, and a `give` inside it must not replay the block it is in.
        current->activeFinallys.pop_back();

        compileStmt(stmt.finallyBlock);
        
        // Load pending exception
        emitLoadLocal(pendingSlot);
        
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
    // The ceiling is now the 16-bit slot that LOAD_LOCAL_W/STORE_LOCAL_W carry,
    // not the single byte the narrow opcodes use. 256 was reachable in ordinary
    // code -- a module accumulates one local per imported symbol, and ezsqlite's
    // connection.ez exceeded it just by importing its own siblings.
    if (current->locals.size() > 65535) {
        errorAt("Too many local variables in function (max 65535)", currentLine);
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

    // LOAD_UPVALUE/STORE_UPVALUE address the closure's upvalue list with a
    // single byte, so stop at 255 rather than let the index wrap and bind the
    // closure to the wrong captured variable.
    if (current->upvalues.size() >= 255) {
        errorAt("Too many captured variables in one function (max 255)", currentLine);
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

// Local access, narrow when it fits in a byte and wide when it does not.
//
// The single-byte operand is why a function was capped at 256 locals. Rather
// than widen every local access -- which would cost a byte on the hottest
// opcodes in the language for the sake of a case almost nobody hits -- the wide
// form is emitted only for slots past 255.
void BytecodeCompiler::emitLoadLocal(size_t slot) {
    if (slot <= 0xFF) {
        // Raw emit, NOT emitLoadLocal() -- this is the function that decides.
        emitBytes(static_cast<uint8_t>(OpCode::LOAD_LOCAL), static_cast<uint8_t>(slot));
    } else {
        emitOp(OpCode::LOAD_LOCAL_W);
        emitBytes(static_cast<uint8_t>((slot >> 8) & 0xFF), static_cast<uint8_t>(slot & 0xFF));
    }
}

void BytecodeCompiler::emitStoreLocal(size_t slot) {
    if (slot <= 0xFF) {
        // Raw emit, NOT emitStoreLocal() -- this is the function that decides.
        emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL), static_cast<uint8_t>(slot));
    } else {
        emitOp(OpCode::STORE_LOCAL_W);
        emitBytes(static_cast<uint8_t>((slot >> 8) & 0xFF), static_cast<uint8_t>(slot & 0xFF));
    }
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

// Warn that an assignment will overwrite a runtime builtin.
//
// A warning rather than an error: replacing a builtin is legal, and a program
// may do it deliberately. But it is almost never intended inside a helper, so
// staying silent is worse -- the symptom appears far from the cause.
//
// Reported once per name per compilation. `num = num + 1` in a loop is one
// mistake, not one per line.
void BytecodeCompiler::warnBuiltinAssignment(const std::string& name, size_t line) {
    if (builtinNames.find(name) == builtinNames.end()) return;
    if (!warnedBuiltins.insert(name).second) return;

    std::cerr << "\033[33mWarning\033[0m";
    if (!currentFile.empty()) std::cerr << " in " << currentFile;
    std::cerr << " at line " << line << ":\n"
              << "  assigning to '" << name << "' replaces the builtin of that name\n"
              << "  for the whole program, so later calls to " << name
              << "(...) will fail.\n"
              << "  Hint: rename the variable, or write '" << name
              << ": <type> = ...' to declare a real local.\n";
}

void BytecodeCompiler::errorAt(const std::string& message, int line) {
    if (hadError) return;
    hadError = true;

    // "[3] Could not find module 'htmp'" gives a line number with no file, which
    // in a program spread over several modules is not enough to find. Report the
    // kind first, then the location, then whatever detail the caller supplied --
    // the same shape the runtime errors use, so both read alike.
    std::string headline = message;
    std::string detail;
    size_t breakAt = message.find('\n');
    if (breakAt != std::string::npos) {
        headline = message.substr(0, breakAt);
        detail = message.substr(breakAt);          // keeps its leading newline
    }

    // Messages that already name their kind ("ModuleNotFoundError: …") keep it;
    // everything else gets the generic one so no compile error is untyped.
    if (headline.find("Error:") == std::string::npos &&
        headline.find("Warning:") == std::string::npos) {
        headline = "CompileError: " + headline;
    }

    std::string where = currentFile.empty() ? std::string("<main>") : currentFile;
    errorMessage = headline + "\n  at " + where + ":" + std::to_string(line) + detail;

    // Show the offending source line when the file has been registered, which
    // is what makes a module error point at the `use` rather than at a number.
    // Declared locally rather than by including the VM header, which includes
    // this compiler's header in turn.
    extern const std::string* EZ_GetSourceLine(const std::string&, int);
    if (const std::string* text = EZ_GetSourceLine(where, line)) {
        std::string trimmed = *text;
        size_t first = trimmed.find_first_not_of(" \t");
        if (first != std::string::npos) trimmed = trimmed.substr(first);
        if (!trimmed.empty()) {
            size_t insertAt = errorMessage.find('\n', headline.size() + 1);
            std::string snippet = "\n      " + trimmed;
            if (insertAt == std::string::npos) errorMessage += snippet;
            else errorMessage.insert(insertAt, snippet);
        }
    }

    throw CompilerError(errorMessage); // Throw to immediately abort current compilation path
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


