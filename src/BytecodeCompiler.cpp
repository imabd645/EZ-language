#include "BytecodeCompiler.h"
#include <iostream>
#include <chrono>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
namespace fs = std::filesystem;
#include "Lexer.h"
#include "Parser.h"

std::unordered_map<std::string, std::string> BytecodeCompiler::virtualFileSystem;

// Helper to get directory of a file
static std::string getDirectoryName(const std::string& path) {
    size_t lastSlash = path.find_last_of("\\/");
    if (lastSlash == std::string::npos) return ".";
    return path.substr(0, lastSlash);
}

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

BytecodeCompiler::BytecodeCompiler() : current(nullptr), currentLine(0), hadError(false), nextGlobalSlot(0) {}

uint16_t BytecodeCompiler::globalSlotFor(const std::string& name) {
    auto it = globalSlots.find(name);
    if (it != globalSlots.end()) return it->second;
    uint16_t slot = nextGlobalSlot++;
    globalSlots[name] = slot;
    return slot;
}

CompileResult BytecodeCompiler::compile(const std::vector<StmtPtr>& statements) {
    CompileResult result;
    hadError = false;
    errorMessage.clear();
    compiledFunctions.clear();

    // Create main function compiler
    current = new Compiler("<main>", 0, nullptr);

    // Compile all statements
    for (const auto& stmt : statements) {
        if (hadError) break;
        compileStmt(stmt);
    }

    // Emit implicit nil return for main
    if (!hadError) {
        emitOp(OpCode::LOAD_NIL);
        emitReturn();
    }

    if (hadError) {
        result.success = false;
        result.error = errorMessage;
        delete current;
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

    delete current;
    current = nullptr;

    return result;
}

BytecodeFunctionPtr BytecodeCompiler::compileFunction(const TaskStmt& task,
                                                       const std::string& name) {
    // Save enclosing compiler
    Compiler* enclosing = current;

    // Create a new compiler scope for this function
    current = new Compiler(name, task.params.size(), enclosing);
    
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

    // Add parameters as the first locals (slot 0, 1, 2, …)
    for (const auto& param : task.params) {
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
            emitBytes(static_cast<uint8_t>(OpCode::LOAD_LOCAL),
                      static_cast<uint8_t>(i));
            emitOp(OpCode::LOAD_NIL);
            emitOp(OpCode::EQUAL);
            size_t jump = emitJump(OpCode::JUMP_IF_FALSE);

            compileExpr(defaultValue);
            emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL),
                      static_cast<uint8_t>(i));
            emitOp(OpCode::POP); // pop result of STORE_LOCAL (peek design)

            patchJump(jump);
        }
    }

    beginScope();
    for (const auto& stmt : task.body) {
        compileStmt(stmt);
    }
    endScope();

    // Implicit nil return
    emitOp(OpCode::LOAD_NIL);
    emitReturn();

    BytecodeFunctionPtr result = current->function;
    result->localCount = current->maxLocals;
    // Propagate the source filename so stack traces can show which file this function is from
    result->filename = currentFile;

    // Register this function in compiledFunctions so the VM can find it.
    // The index assigned here is what CLOSURE will use.
    compiledFunctions.push_back(result);

    delete current;
    current = enclosing;

    return result;
}

// ============================================================================
// Expression Compilation
// ============================================================================

void BytecodeCompiler::compileExpr(const ExprPtr& expr) {
    if (!expr) {
        emitOp(OpCode::LOAD_NIL);
        return;
    }

    currentLine = expr->line;
    currentFile = expr->filename;

    std::visit([this](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, std::shared_ptr<LiteralExpr>>) {
            compileLiteral(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<IdentifierExpr>>) {
            compileIdentifier(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<BinaryExpr>>) {
            compileBinary(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<UnaryExpr>>) {
            compileUnary(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<CallExpr>>) {
            compileCall(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<IndexExpr>>) {
            compileIndex(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<ArrayExpr>>) {
            compileArray(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<AssignExpr>>) {
            compileAssign(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<LogicalExpr>>) {
            compileLogical(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<TernaryExpr>>) {
            compileTernary(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<LambdaExpr>>) {
            compileLambda(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<PropertyAccessExpr>>) {
            compilePropertyAccess(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<SelfExpr>>) {
            compileSelf(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<NewExpr>>) {
            compileNew(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<SetExpr>>) {
            compileSet(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<DictionaryExpr>>) {
            compileDictionary(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<SpreadExpr>>) {
            compileSpread(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<AwaitExpr>>) {
            compileAwait(*arg);
        }
    }, expr->variant);
}

void BytecodeCompiler::compileLiteral(const LiteralExpr& expr) {
    std::visit([this](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            emitOp(OpCode::LOAD_NIL);
        } else if constexpr (std::is_same_v<T, bool>) {
            emitOp(arg ? OpCode::LOAD_TRUE : OpCode::LOAD_FALSE);
        } else if constexpr (std::is_same_v<T, long long>) {
            if (arg == 0) emitOp(OpCode::LOAD_ZERO);
            else if (arg == 1) emitOp(OpCode::LOAD_ONE);
            else emitConstant(Constant(arg));
        } else if constexpr (std::is_same_v<T, double>) {
            emitConstant(Constant(arg));
        } else if constexpr (std::is_same_v<T, std::string>) {
            if (arg.empty()) emitOp(OpCode::LOAD_EMPTY_STR);
            else emitConstant(Constant(arg));
        }
    }, expr.value);
}

void BytecodeCompiler::compileIdentifier(const IdentifierExpr& expr) {
    // Check if it's a static variable in current or parent functions
    std::string mangled = resolveStatic(expr.name);
    if (!mangled.empty()) {
        size_t nameIdx = identifierConstant(mangled);
        emitOp(OpCode::LOAD_GLOBAL);
        emitBytes(static_cast<uint8_t>((nameIdx >> 8) & 0xFF),
                  static_cast<uint8_t>(nameIdx & 0xFF));
        return;
    }

    int local = resolveLocal(expr.name);
    if (local != -1) {
        emitBytes(static_cast<uint8_t>(OpCode::LOAD_LOCAL),
                  static_cast<uint8_t>(local));
        return;
    }

    int upvalue = resolveUpvalue(expr.name);
    if (upvalue != -1) {
        emitBytes(static_cast<uint8_t>(OpCode::LOAD_UPVALUE),
                  static_cast<uint8_t>(upvalue));
        return;
    }

    if (expr.name == "self") {
        emitLoadSelf();
        return;
    }

    if (expr.name == "super") {
        if (!current->currentParentClass.empty()) {
            // LOAD self (handles closure capture if needed)
            emitLoadSelf();
            
            // LOAD parent class via slot if known, else fall back to string LOAD_GLOBAL
            auto parentIt = globalSlots.find(current->currentParentClass);
            if (parentIt != globalSlots.end()) {
                uint16_t slot = parentIt->second;
                emitOp(OpCode::LOAD_GLOBAL_SLOT);
                emitBytes(static_cast<uint8_t>((slot >> 8) & 0xFF),
                          static_cast<uint8_t>(slot & 0xFF));
            } else {
                size_t parentIdx = identifierConstant(current->currentParentClass);
                emitOp(OpCode::LOAD_GLOBAL);
                emitBytes(static_cast<uint8_t>((parentIdx >> 8) & 0xFF),
                          static_cast<uint8_t>(parentIdx & 0xFF));
            }
            
            emitOp(OpCode::SUPER);
            return;
        } else {
            error("Cannot use 'super' outside of a model with a parent.");
            return;
        }
    }

    // Emit fast slot-based global load for all known globals
    uint16_t slot = globalSlotFor(expr.name);
    emitOp(OpCode::LOAD_GLOBAL_SLOT);
    emitBytes(static_cast<uint8_t>((slot >> 8) & 0xFF),
              static_cast<uint8_t>(slot & 0xFF));
}

void BytecodeCompiler::compileBinary(const BinaryExpr& expr) {
    // Short-circuit logical operators
    if (expr.op == TokenType::AND || expr.op == TokenType::OR) {
        compileLogicalShortCircuit(expr);
        return;
    }

    compileExpr(expr.left);
    compileExpr(expr.right);

    switch (expr.op) {
        case TokenType::PLUS:          emitOp(OpCode::ADD);        break;
        case TokenType::MINUS:         emitOp(OpCode::SUB);        break;
        case TokenType::STAR:          emitOp(OpCode::MUL);        break;
        case TokenType::SLASH:         emitOp(OpCode::DIV);        break;
        case TokenType::PERCENT:       emitOp(OpCode::MOD);        break;
        case TokenType::AMPERSAND:     emitOp(OpCode::BIT_AND);    break;
        case TokenType::PIPE:          emitOp(OpCode::BIT_OR);     break;
        case TokenType::CARET:         emitOp(OpCode::BIT_XOR);   break;
        case TokenType::LSHIFT:        emitOp(OpCode::SHIFT_LEFT); break;
        case TokenType::RSHIFT:        emitOp(OpCode::SHIFT_RIGHT);break;
        case TokenType::EQUAL_EQUAL:   emitOp(OpCode::EQUAL);      break;
        case TokenType::BANG_EQUAL:    emitOp(OpCode::NOT_EQUAL);  break;
        case TokenType::LESS:          emitOp(OpCode::LESS);       break;
        case TokenType::LESS_EQUAL:    emitOp(OpCode::LESS_EQ);    break;
        case TokenType::GREATER:       emitOp(OpCode::GREATER);    break;
        case TokenType::GREATER_EQUAL: emitOp(OpCode::GREATER_EQ); break;
        default:
            error("Unknown binary operator: " + std::to_string(static_cast<int>(expr.op)));
    }
}

void BytecodeCompiler::compileLogicalShortCircuit(const BinaryExpr& expr) {
    compileExpr(expr.left);
    // DUP top — keep one copy for "leave on stack" if short-circuiting
    emitOp(OpCode::DUP);

    size_t jumpOffset;
    if (expr.op == TokenType::AND) {
        jumpOffset = emitJump(OpCode::JUMP_IF_FALSE);
    } else {
        jumpOffset = emitJump(OpCode::JUMP_IF_TRUE);
    }
    // Jump consumed the dup'd copy; now pop the original left operand
    emitOp(OpCode::POP);
    compileExpr(expr.right);

    patchJump(jumpOffset);
}

void BytecodeCompiler::compileUnary(const UnaryExpr& expr) {
    compileExpr(expr.operand);
    switch (expr.op) {
        case TokenType::MINUS: emitOp(OpCode::NEGATE);  break;
        case TokenType::NOT:   emitOp(OpCode::NOT);     break;
        case TokenType::TILDE: emitOp(OpCode::BIT_NOT); break;
        default: error("Unknown unary operator");
    }
}

void BytecodeCompiler::compileAwait(const AwaitExpr& expr) {
    compileExpr(expr.expression);
    emitOp(OpCode::OP_AWAIT);
}

void BytecodeCompiler::compileCall(const CallExpr& expr) {
    bool hasSpread = false;
    for (const auto& arg : expr.arguments) {
        if (std::holds_alternative<std::shared_ptr<SpreadExpr>>(arg->variant)) {
            hasSpread = true;
            break;
        }
    }

    compileExpr(expr.callee);
    
    if (!hasSpread) {
        // Fast path for normal calls
        for (const auto& arg : expr.arguments) compileExpr(arg);
        emitBytes(static_cast<uint8_t>(OpCode::CALL),
                  static_cast<uint8_t>(expr.arguments.size()));
    } else {
        // Slow path for spread calls: pack all arguments into a single array
        emitBytes(static_cast<uint8_t>(OpCode::MAKE_ARRAY), 0);
        for (const auto& arg : expr.arguments) {
            if (std::holds_alternative<std::shared_ptr<SpreadExpr>>(arg->variant)) {
                auto spread = std::get<std::shared_ptr<SpreadExpr>>(arg->variant);
                compileExpr(spread->expression);
                emitOp(OpCode::ARRAY_EXTEND);
            } else {
                compileExpr(arg);
                emitOp(OpCode::ARRAY_APPEND);
            }
        }
        emitOp(OpCode::CALL_SPREAD);
    }
}

void BytecodeCompiler::compileIndex(const IndexExpr& expr) {
    compileExpr(expr.object);
    compileExpr(expr.index);
    emitOp(OpCode::INDEX_GET);
}

void BytecodeCompiler::compileArray(const ArrayExpr& expr) {
    bool hasSpread = false;
    for (const auto& elem : expr.elements) {
        if (std::holds_alternative<std::shared_ptr<SpreadExpr>>(elem->variant)) {
            hasSpread = true;
            break;
        }
    }

    if (!hasSpread) {
        // Fast path for normal arrays
        for (const auto& elem : expr.elements) compileExpr(elem);
        emitBytes(static_cast<uint8_t>(OpCode::MAKE_ARRAY),
                  static_cast<uint8_t>(expr.elements.size()));
    } else {
        // Slow path for spread arrays
        emitBytes(static_cast<uint8_t>(OpCode::MAKE_ARRAY), 0);
        for (const auto& elem : expr.elements) {
            if (std::holds_alternative<std::shared_ptr<SpreadExpr>>(elem->variant)) {
                auto spread = std::get<std::shared_ptr<SpreadExpr>>(elem->variant);
                compileExpr(spread->expression);
                emitOp(OpCode::ARRAY_EXTEND);
            } else {
                compileExpr(elem);
                emitOp(OpCode::ARRAY_APPEND);
            }
        }
    }
}

void BytecodeCompiler::compileAssign(const AssignExpr& expr) {
    if (expr.index) {
        // arr[idx] = val  — INDEX_SET leaves value on stack already
        compileExpr(expr.object);
        compileExpr(expr.index);
        compileExpr(expr.value);
        emitOp(OpCode::INDEX_SET);
    } else {
        compileExpr(expr.value);

        // Check if it's a static variable in current or parent functions
        std::string mangled = resolveStatic(expr.name);
        if (!mangled.empty()) {
            uint16_t slot = globalSlotFor(mangled);
            emitOp(OpCode::STORE_GLOBAL_SLOT);
            emitBytes(static_cast<uint8_t>((slot >> 8) & 0xFF),
                      static_cast<uint8_t>(slot & 0xFF));
            return;
        }

        int local = resolveLocal(expr.name);
        if (local != -1) {
            emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL),
                      static_cast<uint8_t>(local));
        } else {
            int upvalue = resolveUpvalue(expr.name);
            if (upvalue != -1) {
                emitBytes(static_cast<uint8_t>(OpCode::STORE_UPVALUE),
                          static_cast<uint8_t>(upvalue));
            } else if (globalSlots.count(expr.name) > 0 && !current->isHarvesting) {
                // Known global — use fast slot path
                uint16_t slot = globalSlots[expr.name];
                emitOp(OpCode::STORE_GLOBAL_SLOT);
                emitBytes(static_cast<uint8_t>((slot >> 8) & 0xFF),
                          static_cast<uint8_t>(slot & 0xFF));
            } else if (current->scopeDepth > 0) {
                // Create a new local if we're in a function or module scope
                local = addLocal(expr.name);
                current->locals.back().isStackResident = false; // Assignment creation doesn't stay on stack
                markInitialized();
                emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL),
                          static_cast<uint8_t>(local));
            } else {
                // Default to global — allocate a slot
                uint16_t slot = globalSlotFor(expr.name);
                emitOp(OpCode::STORE_GLOBAL_SLOT);
                emitBytes(static_cast<uint8_t>((slot >> 8) & 0xFF),
                          static_cast<uint8_t>(slot & 0xFF));
            }
        }
        // The popped value caused STORE to consume the dup; original stays
    }
}

void BytecodeCompiler::compileLogical(const LogicalExpr& expr) {
    compileExpr(expr.left);
    emitOp(OpCode::DUP);

    size_t jumpOffset;
    if (expr.op == TokenType::AND) {
        jumpOffset = emitJump(OpCode::JUMP_IF_FALSE);
    } else {
        jumpOffset = emitJump(OpCode::JUMP_IF_TRUE);
    }
    emitOp(OpCode::POP);
    compileExpr(expr.right);

    patchJump(jumpOffset);
}

void BytecodeCompiler::compileTernary(const TernaryExpr& expr) {
    compileExpr(expr.condition);
    size_t elseJump = emitJump(OpCode::JUMP_IF_FALSE);

    compileExpr(expr.thenBranch);
    size_t endJump = emitJump(OpCode::JUMP);

    patchJump(elseJump);
    compileExpr(expr.elseBranch);

    patchJump(endJump);
}

void BytecodeCompiler::compileLambda(const LambdaExpr& expr) {
    std::string name = "<lambda>";

    TaskStmt fakeTask(name, expr.params, std::vector<TypeASTPtr>(expr.params.size(), std::make_shared<TypeAST>("Any")), std::vector<ExprPtr>{}, nullptr,
                      expr.body ? std::vector<StmtPtr>{} : expr.stmtBody,
                      expr.isVariadic, expr.isAsync);

    if (expr.body) {
        fakeTask.body.push_back(makeGiveStmt(0, 0, 0, "", expr.body));
    }

    // Record how many functions exist before compiling (so we know the index)
    size_t funcIdx = compiledFunctions.size();
    BytecodeFunctionPtr func = compileFunction(fakeTask, name);

    // Store a reference in the enclosing function's nestedFunctions table
    // so the VM can resolve CLOSURE <n> at runtime.
    if (current) {
        size_t nestedIdx = current->function->nestedFunctions.size();
    current->function->nestedFunctions.push_back(func);

        emitOp(OpCode::CLOSURE);
        emitBytes(static_cast<uint8_t>((nestedIdx >> 8) & 0xFF),
                  static_cast<uint8_t>(nestedIdx & 0xFF));

        // Emit upvalue capture descriptors
        for (const auto& uv : func->upvalues) {
            emitByte(uv.type == Upvalue::Type::LOCAL ? 1 : 0);
            emitByte(static_cast<uint8_t>(uv.index));
        }
    }
}

void BytecodeCompiler::compilePropertyAccess(const PropertyAccessExpr& expr) {
    compileExpr(expr.object);
    size_t nameIdx = identifierConstant(expr.property);
    emitOp(OpCode::LOAD_PROPERTY);
    emitBytes(static_cast<uint8_t>((nameIdx >> 8) & 0xFF),
              static_cast<uint8_t>(nameIdx & 0xFF));
}

void BytecodeCompiler::compileSelf(const SelfExpr& /*expr*/) {
    emitLoadSelf();
}

void BytecodeCompiler::emitLoadSelf() {
    int local = resolveLocal("self");
    if (local != -1) {
        emitBytes(static_cast<uint8_t>(OpCode::LOAD_LOCAL), static_cast<uint8_t>(local));
        return;
    }

    int upvalue = resolveUpvalue("self");
    if (upvalue != -1) {
        emitBytes(static_cast<uint8_t>(OpCode::LOAD_UPVALUE), static_cast<uint8_t>(upvalue));
        return;
    }

    error("Cannot use 'self' or 'super' outside of a model method");
}

void BytecodeCompiler::compileNew(const NewExpr& expr) {
    for (const auto& arg : expr.arguments) {
        compileExpr(arg);
    }
    size_t modelIdx = identifierConstant(expr.className);
    emitOp(OpCode::NEW_INSTANCE);
    emitBytes(static_cast<uint8_t>((modelIdx >> 8) & 0xFF),
              static_cast<uint8_t>(modelIdx & 0xFF));
    emitByte(static_cast<uint8_t>(expr.arguments.size()));
}

void BytecodeCompiler::compileSet(const SetExpr& expr) {
    compileExpr(expr.object);
    compileExpr(expr.value);
    size_t nameIdx = identifierConstant(expr.name);
    emitOp(OpCode::STORE_PROPERTY);
    emitBytes(static_cast<uint8_t>((nameIdx >> 8) & 0xFF),
              static_cast<uint8_t>(nameIdx & 0xFF));
}

void BytecodeCompiler::compileDictionary(const DictionaryExpr& expr) {
    for (const auto& [key, value] : expr.pairs) {
        compileExpr(key);
        compileExpr(value);
    }
    emitBytes(static_cast<uint8_t>(OpCode::MAKE_DICT),
              static_cast<uint8_t>(expr.pairs.size()));
}

void BytecodeCompiler::compileSpread(const SpreadExpr& expr) {
    error("Spread expressions are only allowed inside arrays or function calls.");
}

// ============================================================================
// Statement Compilation
// ============================================================================

void BytecodeCompiler::compileStmt(const StmtPtr& stmt) {
    if (!stmt) return;

    currentLine = stmt->line;
    currentFile = stmt->filename;

    std::visit([this](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, std::shared_ptr<ExprStmt>>) {
            compileExprStmt(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<OutStmt>>) {
            compileOutStmt(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<VarDeclStmt>>) {
            compileVarDecl(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<BlockStmt>>) {
            compileBlock(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<WhenStmt>>) {
            compileWhen(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<WhileStmt>>) {
            compileWhile(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<RepeatStmt>>) {
            compileRepeat(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<GetStmt>>) {
            compileGet(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<MatchStmt>>) {
            compileMatch(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<TaskStmt>>) {
            compileTask(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<GiveStmt>>) {
            compileGive(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<UseStmt>>) {
            compileUse(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<ExportStmt>>) {
            compileExport(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<EscapeStmt>>) {
            compileEscape(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<SkipStmt>>) {
            compileSkip(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<ModelStmt>>) {
            compileModel(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<StaticStmt>>) {
            compileStatic(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<TryStmt>>) {
            compileTry(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<ThrowStmt>>) {
            compileThrow(*arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<InterfaceStmt>>) {
            compileInterface(*arg);
        }
    }, stmt->variant);
}

void BytecodeCompiler::compileExprStmt(const ExprStmt& stmt) {
    compileExpr(stmt.expression);
    emitOp(OpCode::POP);
}

void BytecodeCompiler::compileOutStmt(const OutStmt& stmt) {
    compileExpr(stmt.expression);
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

    // Try to detect loop direction if both start and end are literals
    bool isReverse = false;
    auto startLit = std::get_if<std::shared_ptr<LiteralExpr>>(&stmt.start->variant);
    auto endLit = std::get_if<std::shared_ptr<LiteralExpr>>(&stmt.end->variant);
    if (startLit && endLit) {
        if ((*startLit)->value.index() == 1 && (*endLit)->value.index() == 1) { // DOUBLE
            if (std::get<double>((*startLit)->value) > std::get<double>((*endLit)->value)) isReverse = true;
        } else if ((*startLit)->value.index() == 2 && (*endLit)->value.index() == 2) { // INT (long long)
            if (std::get<long long>((*startLit)->value) > std::get<long long>((*endLit)->value)) isReverse = true;
        }
    }

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

    startLoop();
    size_t loopStart = currentChunk().code.size();
    loopStack.back().start = loopStart;

    // Condition: loopVar <= endVar (forward) or loopVar >= endVar (reverse)
    emitBytes(static_cast<uint8_t>(OpCode::LOAD_LOCAL),
              static_cast<uint8_t>(loopVar));
    emitBytes(static_cast<uint8_t>(OpCode::LOAD_LOCAL),
              static_cast<uint8_t>(endVar));
    
    if (isReverse) {
        emitOp(OpCode::GREATER_EQ);
    } else {
        emitOp(OpCode::LESS_EQ);
    }

    size_t exitJump = emitJump(OpCode::JUMP_IF_FALSE);

    compileStmt(stmt.body);

    // Increment/Decrement loopVar
    if (isReverse) {
        emitBytes(static_cast<uint8_t>(OpCode::DEC_LOCAL),
                  static_cast<uint8_t>(loopVar));
    } else {
        emitBytes(static_cast<uint8_t>(OpCode::INC_LOCAL),
                  static_cast<uint8_t>(loopVar));
    }

    emitLoop(loopStart);
    patchJump(exitJump);

    // Retrieve loop context again as the stack may have reallocated
    for (size_t breakOffset : loopStack.back().breaks) patchJump(breakOffset);
    endLoop();
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
        compileExpr(std::make_shared<Expr>(currentLine, 0, 0, currentFile, std::make_shared<LiteralExpr>(0LL)));
        emitOp(OpCode::INDEX_GET); // [array, key]
        emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL), static_cast<uint8_t>(loopVar));
        emitOp(OpCode::POP); // [array]
        
        // value = array[1]
        compileExpr(std::make_shared<Expr>(currentLine, 0, 0, currentFile, std::make_shared<LiteralExpr>(1LL)));
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

    for (const auto& arm : stmt.arms) {
        if (!arm.pattern) {
            // Default arm ('other')
            // Pop the subject since we matched it
            emitOp(OpCode::POP);
            compileStmt(arm.body);
            endJumps.push_back(emitJump(OpCode::JUMP));
        } else {
            // DUP the subject
            emitOp(OpCode::DUP);
            compileExpr(arm.pattern);
            emitOp(OpCode::EQUAL);

            size_t nextArmJump = emitJump(OpCode::JUMP_IF_FALSE); // This pops the boolean result
            
            // Match! Pop the subject
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
        error("emitClosure: no active compiler");
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
    emitClosure(stmt);

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
    if (stmt.value) {
        /* 
        // Optimization: if the value is a function call, we can emit OpCode::TAIL_CALL
        if (std::holds_alternative<std::shared_ptr<CallExpr>>(stmt.value->variant)) {
            auto call = std::get<std::shared_ptr<CallExpr>>(stmt.value->variant);
            compileExpr(call->callee);
            for (const auto& arg : call->arguments) {
                compileExpr(arg);
            }
            emitBytes(static_cast<uint8_t>(OpCode::TAIL_CALL),
                      static_cast<uint8_t>(call->arguments.size()));
            return;
        }
        */
        compileExpr(stmt.value);
    } else {
        emitOp(OpCode::LOAD_NIL);
    }
    emitReturn();
}

void BytecodeCompiler::compileEscape(const EscapeStmt& /*stmt*/) {
    if (loopStack.empty()) {
        error("'escape' outside of loop");
        return;
    }
    emitBreak();
}

void BytecodeCompiler::compileSkip(const SkipStmt& /*stmt*/) {
    if (loopStack.empty()) {
        error("'skip' outside of loop");
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
    } else if (virtualFileSystem.count("C:/ezlib/" + path)) {
        source = virtualFileSystem["C:/ezlib/" + path];
        absolutePath = "C:/ezlib/" + path;
        foundInVFS = true;
    } else if (virtualFileSystem.count("C:/ezlib/" + path + ".ez")) {
        source = virtualFileSystem["C:/ezlib/" + path + ".ez"];
        absolutePath = "C:/ezlib/" + path + ".ez";
        foundInVFS = true;
    } else if (virtualFileSystem.count("C:/ezlib/" + path + "/main.ez")) {
        source = virtualFileSystem["C:/ezlib/" + path + "/main.ez"];
        absolutePath = "C:/ezlib/" + path + "/main.ez";
        foundInVFS = true;
    }
    
    if (!foundInVFS) {
        std::ifstream file(absolutePath);
        if (!file.is_open()) {
            // Try exactly as typed
            file.open(path);
            if (file.is_open()) {
                absolutePath = path;
            } else {
                // Try local lib/ directory
                std::string localLibPath = "lib/" + path;
                file.open(localLibPath);
                if (file.is_open()) {
                    absolutePath = localLibPath;
                } else {
                    // Try standard lib path
                    std::string libPath = "C:/ezlib/" + path;
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
                                error("Could not find entry point in module directory '" + libPath + "'");
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
                                error("Could not find module '" + path + "'");
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
    
    static std::unordered_set<std::string> compilingModules;
    if (compilingModules.count(absolutePath)) {
        error("Circular dependency detected when importing '" + absolutePath + "'");
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
    static std::unordered_map<std::string, std::vector<StmtPtr>> astCache;
    
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
            error("Lexer error in module '" + absolutePath + "'");
            return;
        }
        
        Parser parser(tokens);
        statements = parser.parse();
        if (parser.hasError()) {
            error("Parser error in module '" + absolutePath + "'");
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
        Compiler moduleCompiler(alias + "_module", 0, current);
        Compiler* previous = current;
        current = &moduleCompiler;

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
                
                emitOp(OpCode::STORE_PROPERTY);
                size_t propIdx = identifierConstant(local.name);
                emitBytes(static_cast<uint8_t>((propIdx >> 8) & 0xFF),
                          static_cast<uint8_t>(propIdx & 0xFF));
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
        TaskStmt initTask("init", params, std::vector<TypeASTPtr>(params.size(), std::make_shared<TypeAST>("Any")), defaults, nullptr, stmt.initBody);
        emitClosure(initTask, true); // Pushes closure
        
        // Push isStatic flag (false for constructor)
        emitOp(OpCode::LOAD_FALSE);
        
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
            TaskStmt methodTask(member.name, params, std::vector<TypeASTPtr>(params.size(), std::make_shared<TypeAST>("Any")), defaults, nullptr, member.body, false, member.isAsync);
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
                  
        emitOp(OpCode::LOAD_TRUE);
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

    // 6. Emit MAKE_CLASS nameIdx, memberCount, interfaceCount
    size_t classNameIdx = identifierConstant(stmt.name);
    emitOp(OpCode::MAKE_CLASS);
    emitBytes(static_cast<uint8_t>((classNameIdx >> 8) & 0xFF),
              static_cast<uint8_t>(classNameIdx & 0xFF));
    emitByte(static_cast<uint8_t>(memberCount));
    emitByte(static_cast<uint8_t>(stmt.interfaces.size()));
    
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
    // Emit TRY_START with a placeholder jump offset to the catch handler.
    size_t tryStart = emitJump(OpCode::TRY_START);

    // Compile the try block
    compileStmt(stmt.tryBlock);
    emitOp(OpCode::TRY_END);

    // Jump over catch handlers if no exception was raised
    size_t afterCatch = emitJump(OpCode::JUMP);

    // Patch TRY_START to point here (the catch handler entry)
    patchJump(tryStart);

    // At this point, the exception is on the stack: [exc]
    
    std::vector<size_t> successJumps;

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
                markInitialized();
                // Exception is on stack. Store it into the local variable slot.
                emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL),
                          static_cast<uint8_t>(slot));
                emitOp(OpCode::POP);
            } else {
                emitOp(OpCode::POP); // No variable bound, discard exception
            }
            compileStmt(cb.body);
            endScope();
            
            successJumps.push_back(emitJump(OpCode::JUMP)); // Jump to afterCatch
            
            patchJump(nextCatch); // Point next catch check here
        } else {
            // Catch-all
            beginScope();
            if (!cb.varName.empty()) {
                int slot = addLocal(cb.varName);
                markInitialized();
                // Exception is on stack. Store it into the local variable slot.
                emitBytes(static_cast<uint8_t>(OpCode::STORE_LOCAL),
                          static_cast<uint8_t>(slot));
                emitOp(OpCode::POP);
            } else {
                emitOp(OpCode::POP); // No variable bound, discard exception
            }
            compileStmt(cb.body);
            endScope();
            
            successJumps.push_back(emitJump(OpCode::JUMP));
            
            // Catch-all reached, any subsequent catch blocks are unreachable
            break;
        }
    }
    
    // If we fall through here, it means no catch block matched (and no catch-all was present)
    // Re-throw the exception which is still on the stack
    emitOp(OpCode::THROW);

    // Patch all successful catch handlers to jump here
    for (size_t jump : successJumps) {
        patchJump(jump);
    }
    
    patchJump(afterCatch);
}

void BytecodeCompiler::compileThrow(const ThrowStmt& stmt) {
    compileExpr(stmt.expression);
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
        error("Too many constants in one chunk (max 65535)");
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
    }
}

// ============================================================================
// Loop Handling
// ============================================================================

void BytecodeCompiler::startLoop() {
    LoopContext loop;
    loop.start = currentChunk().code.size();
    loopStack.push_back(loop);
}

void BytecodeCompiler::endLoop() {
    loopStack.pop_back();
}

void BytecodeCompiler::emitBreak() {
    if (loopStack.empty()) { error("'break' outside of loop"); return; }
    // NOTE: do NOT pop a phantom value here; the value stack is balanced
    // at this point by the loop body itself.
    size_t jumpOffset = emitJump(OpCode::JUMP);
    loopStack.back().breaks.push_back(jumpOffset);
}

void BytecodeCompiler::emitContinue() {
    if (loopStack.empty()) { error("'continue' outside of loop"); return; }
    emitLoop(loopStack.back().start);
}


