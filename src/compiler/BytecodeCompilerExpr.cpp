#include "BytecodeCompiler.h"
#include "utils/WrapArith.h"
#include <cmath>
#include <iostream>
void BytecodeCompiler::compileExpr(const ExprPtr& expr) {
    if (!expr) {
        emitOp(OpCode::LOAD_NIL);
        return;
    }

    currentLine = expr->line;
    currentFile = expr->filename;

    std::visit([this](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, LiteralExpr*>) {
            compileLiteral(*arg);
        } else if constexpr (std::is_same_v<T, IdentifierExpr*>) {
            compileIdentifier(*arg);
        } else if constexpr (std::is_same_v<T, BinaryExpr*>) {
            compileBinary(*arg);
        } else if constexpr (std::is_same_v<T, UnaryExpr*>) {
            compileUnary(*arg);
        } else if constexpr (std::is_same_v<T, CallExpr*>) {
            compileCall(*arg);
        } else if constexpr (std::is_same_v<T, IndexExpr*>) {
            compileIndex(*arg);
        } else if constexpr (std::is_same_v<T, ArrayExpr*>) {
            compileArray(*arg);
        } else if constexpr (std::is_same_v<T, TupleExpr*>) {
            compileTuple(*arg);
        } else if constexpr (std::is_same_v<T, DictionaryExpr*>) {
            compileDictionary(*arg);
        } else if constexpr (std::is_same_v<T, AssignExpr*>) {
            compileAssign(*arg);
        } else if constexpr (std::is_same_v<T, DestructureAssignExpr*>) {
            compileDestructureAssign(*arg);
        } else if constexpr (std::is_same_v<T, LogicalExpr*>) {
            compileLogical(*arg);
        } else if constexpr (std::is_same_v<T, TernaryExpr*>) {
            compileTernary(*arg);
        } else if constexpr (std::is_same_v<T, LambdaExpr*>) {
            compileLambda(*arg);
        } else if constexpr (std::is_same_v<T, PropertyAccessExpr*>) {
            compilePropertyAccess(*arg);
        } else if constexpr (std::is_same_v<T, SelfExpr*>) {
            compileSelf(*arg);
        } else if constexpr (std::is_same_v<T, SuperExpr*>) {
            compileSuper(*arg);
        } else if constexpr (std::is_same_v<T, NewExpr*>) {
            compileNew(*arg);
        } else if constexpr (std::is_same_v<T, SetExpr*>) {
            compileSet(*arg);

        } else if constexpr (std::is_same_v<T, SpreadExpr*>) {
            compileSpread(*arg);
        } else if constexpr (std::is_same_v<T, AwaitExpr*>) {
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
        emitLoadLocal(local);
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
            errorAt("Cannot use 'super' outside of a model with a parent.", currentLine);
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

    Constant leftConst, rightConst;
    bool hasLeftConst = isConstant(expr.left, leftConst);
    bool hasRightConst = isConstant(expr.right, rightConst);

    if (hasLeftConst && hasRightConst) {
        BinaryExpr bin(expr.left, expr.op, expr.right);
        Expr tempExpr(0, 0, 0, "", &bin);
        Constant folded;
        if (isConstant(&tempExpr, folded)) {
            emitConstant(folded);
            return;
        }
    }

    // Algebraic Simplifications (Identity Reductions)
    if (hasRightConst) {
        if (rightConst.type == Constant::Type::INT) {
            long long r = std::get<long long>(rightConst.value);
            if (expr.op == TokenType::PLUS && r == 0) {
                compileExpr(expr.left);
                return;
            }
            if (expr.op == TokenType::MINUS && r == 0) {
                compileExpr(expr.left);
                return;
            }
            if (expr.op == TokenType::STAR && r == 1) {
                compileExpr(expr.left);
                return;
            }
        }
    }
    if (hasLeftConst) {
        if (leftConst.type == Constant::Type::INT) {
            long long l = std::get<long long>(leftConst.value);
            if (expr.op == TokenType::PLUS && l == 0) {
                compileExpr(expr.right);
                return;
            }
            if (expr.op == TokenType::STAR && l == 1) {
                compileExpr(expr.right);
                return;
            }
        }
    }

    compileExpr(expr.left);
    compileExpr(expr.right);

    switch (expr.op) {
        case TokenType::PLUS:          emitOp(OpCode::ADD);        break;
        case TokenType::MINUS:         emitOp(OpCode::SUB);        break;
        case TokenType::STAR:          emitOp(OpCode::MUL);        break;
        case TokenType::STAR_STAR:      emitOp(OpCode::POW);        break;
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
        // The parser has always accepted `x in y` at comparison precedence, but
        // nothing compiled it -- the expression reached here and died with
        // "Unknown binary operator: 7". That is what stopped the `test` package
        // from loading at all, since it uses `when not (k in b)`.
        case TokenType::IN:            emitOp(OpCode::MEMBER_IN);  break;
        default:
            errorAt("Unknown binary operator: " + std::to_string(static_cast<int>(expr.op)), currentLine);
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
    Constant operandConst;
    if (isConstant(expr.operand, operandConst)) {
        UnaryExpr un(expr.op, expr.operand);
        Expr tempExpr(0, 0, 0, "", &un);
        Constant folded;
        if (isConstant(&tempExpr, folded)) {
            emitConstant(folded);
            return;
        }
    }
    compileExpr(expr.operand);
    switch (expr.op) {
        case TokenType::MINUS: emitOp(OpCode::NEGATE);  break;
        case TokenType::NOT:
        case TokenType::BANG:  emitOp(OpCode::NOT);     break;
        case TokenType::TILDE: emitOp(OpCode::BIT_NOT); break;
        default: errorAt("Unknown unary operator", currentLine);
    }
}

void BytecodeCompiler::compileAwait(const AwaitExpr& expr) {
    compileExpr(expr.expression);
    emitOp(OpCode::OP_AWAIT);
}

// How the source spelled the thing being called, for error messages.
//
// Returns "" for a callee with no name to report -- an immediately-invoked
// lambda, an element of an array of handlers. Those are rare, and inventing a
// name for them would be worse than saying nothing.
std::string BytecodeCompiler::calleeDisplayName(const ExprPtr& callee) {
    if (!callee) return "";

    if (auto* id = std::get_if<IdentifierExpr*>(&callee->variant))
        return (*id)->name;

    if (auto* prop = std::get_if<PropertyAccessExpr*>(&callee->variant)) {
        std::string owner = calleeDisplayName((*prop)->object);
        if (owner.empty()) return (*prop)->property;
        return owner + "." + (*prop)->property;
    }

    if (std::get_if<SelfExpr*>(&callee->variant)) return "self";
    if (std::get_if<SuperExpr*>(&callee->variant)) return "super";

    // An index into a table of functions -- handlers["save"](x). The container
    // is worth naming even though the element is not.
    if (auto* idx = std::get_if<IndexExpr*>(&callee->variant)) {
        std::string owner = calleeDisplayName((*idx)->object);
        if (!owner.empty()) return owner + "[...]";
    }
    return "";
}

// Record what was called, keyed by the offset just past the call instruction,
// which is what frame->ip holds when a call fails.
void BytecodeCompiler::recordCallSite(const std::string& name) {
    if (name.empty()) return;
    current->function->callSites.push_back(
        CallSiteInfo{ static_cast<uint32_t>(currentChunk().code.size()), name });
}

void BytecodeCompiler::compileCall(const CallExpr& expr) {
    // ---- Intercept old(expr) in ensures clauses ----
    // old(expr) is not a real function — it refers to the value captured at function entry.
    if (!oldCaptures.empty()) {
        if (auto* id = std::get_if<IdentifierExpr*>(&expr.callee->variant)) {
            if ((*id)->name == "old" && expr.arguments.size() == 1) {
                // Build the same key used during capture
                std::string key = "old_" + std::to_string(expr.arguments[0]->line) + "_" +
                                           std::to_string(expr.arguments[0]->column);
                auto it = oldCaptures.find(key);
                if (it != oldCaptures.end()) {
                    emitLoadLocal(it->second);
                    return;
                }
            }
        }
    }
    
    bool hasSpread = false;
    for (const auto& arg : expr.arguments) {
        if (std::holds_alternative<SpreadExpr*>(arg->variant)) {
            hasSpread = true;
            break;
        }
    }

    bool hasKwargs = false;
    for (const auto& kw : expr.argNames) {
        if (!kw.empty()) { hasKwargs = true; break; }
    }
    
    if (hasKwargs) {
        compileExpr(expr.callee);
        size_t posCount = 0;
        size_t kwCount = 0;
        
        // Push positional arguments first
        for (size_t i = 0; i < expr.arguments.size(); ++i) {
            if (expr.argNames[i].empty()) {
                compileExpr(expr.arguments[i]);
                posCount++;
            }
        }
        
        // Push keyword arguments (key, value pairs for MAKE_DICT)
        for (size_t i = 0; i < expr.arguments.size(); ++i) {
            if (!expr.argNames[i].empty()) {
                size_t keyIdx = identifierConstant(expr.argNames[i]);
                emitOp(OpCode::LOAD_CONST);
                emitBytes(static_cast<uint8_t>((keyIdx >> 8) & 0xFF),
                          static_cast<uint8_t>(keyIdx & 0xFF));
                compileExpr(expr.arguments[i]);
                kwCount++;
            }
        }
        
        // Build the dictionary
        emitBytes(static_cast<uint8_t>(OpCode::MAKE_DICT),
                  static_cast<uint8_t>(kwCount));
                  
        // Emit CALL_KW with positional argument count
        emitBytes(static_cast<uint8_t>(OpCode::CALL_KW),
                  static_cast<uint8_t>(posCount));
        recordCallSite(calleeDisplayName(expr.callee));
    } else if (!hasSpread) {
        // Fast path for normal calls
        if (expr.arguments.size() > 255) {
            errorAt("Too many arguments (max 255)", currentLine);
            return;
        }

        // Direct method invocation optimization: obj.method(args...)
        if (expr.callee && std::holds_alternative<PropertyAccessExpr*>(expr.callee->variant)) {
            auto prop = std::get<PropertyAccessExpr*>(expr.callee->variant);
            if (!prop->isOptional && prop->object && !std::holds_alternative<SuperExpr*>(prop->object->variant)) {
                compileExpr(prop->object); // Push receiver (self)
                for (const auto& arg : expr.arguments) compileExpr(arg); // Push arguments
                size_t nameIdx = identifierConstant(prop->property);
                size_t icIdx = current->function->chunk.icEntries.size();
                current->function->chunk.icEntries.push_back(ICCacheEntry{});
                emitOp(OpCode::INVOKE_METHOD);
                emitBytes(static_cast<uint8_t>((nameIdx >> 8) & 0xFF),
                          static_cast<uint8_t>(nameIdx & 0xFF));
                emitBytes(static_cast<uint8_t>((icIdx >> 8) & 0xFF),
                          static_cast<uint8_t>(icIdx & 0xFF));
                emitByte(static_cast<uint8_t>(expr.arguments.size()));
                recordCallSite(calleeDisplayName(expr.callee));
                return;
            }
        }

        // Pure builtin constant folding (len, ord, chr, str)
        if (expr.callee && std::holds_alternative<IdentifierExpr*>(expr.callee->variant)) {
            auto id = std::get<IdentifierExpr*>(expr.callee->variant);
            if (resolveLocal(id->name) == -1 && resolveUpvalue(id->name) == -1 && globalSlots.count(id->name) == 0) {
                if (id->name == "len" && expr.arguments.size() == 1) {
                    Constant argConst;
                    if (isConstant(expr.arguments[0], argConst) && argConst.type == Constant::Type::STRING) {
                        emitConstant(Constant(static_cast<long long>(std::get<std::string>(argConst.value).size())));
                        return;
                    }
                } else if (id->name == "ord" && expr.arguments.size() == 1) {
                    Constant argConst;
                    if (isConstant(expr.arguments[0], argConst) && argConst.type == Constant::Type::STRING) {
                        const auto& s = std::get<std::string>(argConst.value);
                        if (!s.empty()) {
                            emitConstant(Constant(static_cast<long long>(static_cast<unsigned char>(s[0]))));
                            return;
                        }
                    }
                } else if (id->name == "chr" && expr.arguments.size() == 1) {
                    Constant argConst;
                    if (isConstant(expr.arguments[0], argConst) && argConst.type == Constant::Type::INT) {
                        long long code = std::get<long long>(argConst.value);
                        if (code >= 0 && code <= 255) {
                            std::string res(1, static_cast<char>(code));
                            emitConstant(Constant(res));
                            return;
                        }
                    }
                } else if (id->name == "str" && expr.arguments.size() == 1) {
                    Constant argConst;
                    if (isConstant(expr.arguments[0], argConst)) {
                        emitConstant(Constant(constantToString(argConst)));
                        return;
                    }
                }
            }
        }

        compileExpr(expr.callee);
        for (const auto& arg : expr.arguments) compileExpr(arg);
        emitBytes(static_cast<uint8_t>(OpCode::CALL),
                  static_cast<uint8_t>(expr.arguments.size()));
        recordCallSite(calleeDisplayName(expr.callee));
    } else {
        compileExpr(expr.callee);
        // Slow path for spread calls: pack all arguments into a single array
        emitBytes(static_cast<uint8_t>(OpCode::MAKE_ARRAY), 0);
        for (const auto& arg : expr.arguments) {
            if (std::holds_alternative<SpreadExpr*>(arg->variant)) {
                auto spread = std::get<SpreadExpr*>(arg->variant);
                compileExpr(spread->expression);
                emitOp(OpCode::ARRAY_EXTEND);
            } else {
                compileExpr(arg);
                emitOp(OpCode::ARRAY_APPEND);
            }
        }
        emitOp(OpCode::CALL_SPREAD);
        recordCallSite(calleeDisplayName(expr.callee));
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
        if (std::holds_alternative<SpreadExpr*>(elem->variant)) {
            hasSpread = true;
            break;
        }
    }

    if (!hasSpread) {
        // Fast path for normal arrays
        if (expr.elements.size() > 255) {
            errorAt("Too many array elements (max 255)", currentLine);
            return;
        }
        for (const auto& elem : expr.elements) compileExpr(elem);
        emitBytes(static_cast<uint8_t>(OpCode::MAKE_ARRAY),
                  static_cast<uint8_t>(expr.elements.size()));
    } else {
        // Slow path for spread arrays
        emitBytes(static_cast<uint8_t>(OpCode::MAKE_ARRAY), 0);
        for (const auto& elem : expr.elements) {
            if (std::holds_alternative<SpreadExpr*>(elem->variant)) {
                auto spread = std::get<SpreadExpr*>(elem->variant);
                compileExpr(spread->expression);
                emitOp(OpCode::ARRAY_EXTEND);
            } else {
                compileExpr(elem);
                emitOp(OpCode::ARRAY_APPEND);
            }
        }
    }
}

void BytecodeCompiler::compileTuple(const TupleExpr& expr) {
    if (expr.elements.size() > 255) {
        errorAt("Too many tuple elements (max 255)", currentLine);
        return;
    }
    for (const auto& elem : expr.elements) {
        compileExpr(elem);
    }
    emitOp(OpCode::BUILD_TUPLE);
    emitByte(static_cast<uint8_t>(expr.elements.size()));
}

void BytecodeCompiler::compileAssign(const AssignExpr& expr) {
    if (expr.index) {
        // arr[idx] = val  — INDEX_SET leaves value on stack already
        compileExpr(expr.object);
        compileExpr(expr.index);
        if (expr.compoundOp.has_value()) {
            emitOp(OpCode::DUP2);
            emitOp(OpCode::INDEX_GET);
            compileExpr(expr.value);
            switch (expr.compoundOp.value()) {
                case TokenType::PLUS_EQUAL: emitOp(OpCode::ADD); break;
                case TokenType::MINUS_EQUAL: emitOp(OpCode::SUB); break;
                case TokenType::STAR_EQUAL: emitOp(OpCode::MUL); break;
                case TokenType::STAR_STAR_EQUAL: emitOp(OpCode::POW); break;
                case TokenType::SLASH_EQUAL: emitOp(OpCode::DIV); break;
                default: emitOp(OpCode::ADD); break;
            }
        } else {
            compileExpr(expr.value);
        }
        emitOp(OpCode::INDEX_SET);
    } else {
        compileExpr(expr.value);

        // Check if it's a static variable in current or parent functions.
        // By name, to match the LOAD_GLOBAL that reads it -- see compileStatic.
        std::string mangled = resolveStatic(expr.name);
        if (!mangled.empty()) {
            size_t sNameIdx = identifierConstant(mangled);
            emitOp(OpCode::STORE_GLOBAL);
            emitBytes(static_cast<uint8_t>((sNameIdx >> 8) & 0xFF),
                      static_cast<uint8_t>(sNameIdx & 0xFF));
            return;
        }

        int local = resolveLocal(expr.name);
        if (local != -1) {
            emitStoreLocal(local);
        } else {
            int upvalue = resolveUpvalue(expr.name);
            if (upvalue != -1) {
                emitBytes(static_cast<uint8_t>(OpCode::STORE_UPVALUE),
                          static_cast<uint8_t>(upvalue));
            } else if (globalSlots.count(expr.name) > 0 && !current->isHarvesting) {
                // Known global — use fast slot path.
                //
                // This branch is reached even inside a function, because the
                // name already has a global slot. That is what makes assigning
                // to a builtin so damaging: `num = 0` in a helper takes this
                // path instead of the scopeDepth > 0 branch below, so it
                // overwrites the function for the entire process rather than
                // creating a local. The failure then appears wherever that
                // builtin is next called -- typically in unrelated code, with
                // nothing pointing back to the assignment.
                warnBuiltinAssignment(expr.name, currentLine);
                uint16_t slot = globalSlots[expr.name];
                emitOp(OpCode::STORE_GLOBAL_SLOT);
                emitBytes(static_cast<uint8_t>((slot >> 8) & 0xFF),
                          static_cast<uint8_t>(slot & 0xFF));
            } else if (current->scopeDepth > 0) {
                // Create a new local if we're in a function or module scope
                local = addLocal(expr.name);
                current->locals.back().isStackResident = false; // Assignment creation doesn't stay on stack
                markInitialized();
                emitStoreLocal(local);
            } else {
                // Default to global — allocate a slot
                warnBuiltinAssignment(expr.name, currentLine);
                uint16_t slot = globalSlotFor(expr.name);
                emitOp(OpCode::STORE_GLOBAL_SLOT);
                emitBytes(static_cast<uint8_t>((slot >> 8) & 0xFF),
                          static_cast<uint8_t>(slot & 0xFF));
            }
        }
        // The popped value caused STORE to consume the dup; original stays
    }
}

void BytecodeCompiler::compileDestructureAssign(const DestructureAssignExpr& expr) {
    // 1. Evaluate RHS: Stack: [..., RHS_val]
    compileExpr(expr.value);
    
    // 2. Store in a temporary local variable: Stack: [..., RHS_val]
    size_t tempVar = addLocal("<destructure-val>");
    current->locals.back().isStackResident = false;
    markInitialized();
    emitStoreLocal(tempVar);
    
    // 3. For each target target_i at index i:
    for (size_t i = 0; i < expr.targets.size(); ++i) {
        const auto& target = expr.targets[i];
        if (!target) {
            continue;
        }
        
        if (std::holds_alternative<IdentifierExpr*>(target->variant)) {
            // Target is an identifier: var
            // Load tempVar
            emitLoadLocal(tempVar);
            // Load index constant i
            int constIdx = (int)makeConstant(Constant(static_cast<long long>(i)));
            emitOp(OpCode::LOAD_CONST);
            emitBytes(static_cast<uint8_t>((constIdx >> 8) & 0xFF), static_cast<uint8_t>(constIdx & 0xFF));
            // INDEX_GET -> leaves element on stack
            emitOp(OpCode::INDEX_GET);
            
            // Store to target variable
            std::string name = std::get<IdentifierExpr*>(target->variant)->name;
            std::string mangled = resolveStatic(name);
            if (!mangled.empty()) {
                // By name, to match the LOAD_GLOBAL that reads it.
                size_t sNameIdx = identifierConstant(mangled);
                emitOp(OpCode::STORE_GLOBAL);
                emitBytes(static_cast<uint8_t>((sNameIdx >> 8) & 0xFF),
                          static_cast<uint8_t>(sNameIdx & 0xFF));
            } else {
                int local = resolveLocal(name);
                if (local != -1) {
                    emitStoreLocal(local);
                } else {
                    int upvalue = resolveUpvalue(name);
                    if (upvalue != -1) {
                        emitBytes(static_cast<uint8_t>(OpCode::STORE_UPVALUE),
                                  static_cast<uint8_t>(upvalue));
                    } else if (globalSlots.count(name) > 0 && !current->isHarvesting) {
                        uint16_t slot = globalSlots[name];
                        emitOp(OpCode::STORE_GLOBAL_SLOT);
                        emitBytes(static_cast<uint8_t>((slot >> 8) & 0xFF),
                                  static_cast<uint8_t>(slot & 0xFF));
                    } else if (current->scopeDepth > 0) {
                        local = addLocal(name);
                        current->locals.back().isStackResident = false;
                        markInitialized();
                        emitStoreLocal(local);
                    } else {
                        uint16_t slot = globalSlotFor(name);
                        emitOp(OpCode::STORE_GLOBAL_SLOT);
                        emitBytes(static_cast<uint8_t>((slot >> 8) & 0xFF),
                                  static_cast<uint8_t>(slot & 0xFF));
                    }
                }
            }
            // Pop the stored value
            emitOp(OpCode::POP);
            
        } else if (std::holds_alternative<IndexExpr*>(target->variant)) {
            // Target is an index expression: obj[index]
            auto indexExpr = std::get<IndexExpr*>(target->variant);
            compileExpr(indexExpr->object);
            compileExpr(indexExpr->index);
            
            // Load element from tempVar
            emitLoadLocal(tempVar);
            int constIdx = (int)makeConstant(Constant(static_cast<long long>(i)));
            emitOp(OpCode::LOAD_CONST);
            emitBytes(static_cast<uint8_t>((constIdx >> 8) & 0xFF), static_cast<uint8_t>(constIdx & 0xFF));
            emitOp(OpCode::INDEX_GET);
            
            // INDEX_SET -> leaves value on stack
            emitOp(OpCode::INDEX_SET);
            emitOp(OpCode::POP);
            
        } else if (std::holds_alternative<PropertyAccessExpr*>(target->variant)) {
            // Target is a property: obj.prop
            auto propExpr = std::get<PropertyAccessExpr*>(target->variant);
            compileExpr(propExpr->object);
            
            // Load element from tempVar
            emitLoadLocal(tempVar);
            int constIdx = (int)makeConstant(Constant(static_cast<long long>(i)));
            emitOp(OpCode::LOAD_CONST);
            emitBytes(static_cast<uint8_t>((constIdx >> 8) & 0xFF), static_cast<uint8_t>(constIdx & 0xFF));
            emitOp(OpCode::INDEX_GET);
            
            // STORE_PROPERTY -> leaves value on stack
            int nameIdx = (int)identifierConstant(propExpr->property);
            emitOp(OpCode::STORE_PROPERTY);
            emitBytes(static_cast<uint8_t>((nameIdx >> 8) & 0xFF), static_cast<uint8_t>(nameIdx & 0xFF));
            size_t icIdx = current->function->chunk.icEntries.size();
            current->function->chunk.icEntries.push_back(ICCacheEntry{});
            emitBytes(static_cast<uint8_t>((icIdx >> 8) & 0xFF), static_cast<uint8_t>(icIdx & 0xFF));
            emitOp(OpCode::POP);
        }
    }
}

void BytecodeCompiler::compileLogical(const LogicalExpr& expr) {
    compileExpr(expr.left);
    emitOp(OpCode::DUP);

    size_t jumpOffset;
    if (expr.op == TokenType::AND) {
        jumpOffset = emitJump(OpCode::JUMP_IF_FALSE);
    } else if (expr.op == TokenType::QUESTION_QUESTION) {
        jumpOffset = emitJump(OpCode::JUMP_IF_NOT_NIL);
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

    TaskStmt fakeTask(name, expr.params, std::vector<TypeASTPtr>(expr.params.size(), arena.allocate<TypeAST>("Any")), std::vector<ExprPtr>{}, nullptr,
                      expr.body ? std::vector<StmtPtr>{} : expr.stmtBody,
                      expr.isVariadic, expr.isAsync);

    if (expr.body) {
        fakeTask.body.push_back(makeGiveStmt(arena, 0, 0, 0, "", expr.body));
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
            // 16-bit: uv.index is a slot in the enclosing function, which can
            // hold more than 256 locals. See BytecodeCompilerStmt.cpp.
            emitBytes(static_cast<uint8_t>((uv.index >> 8) & 0xFF),
                      static_cast<uint8_t>(uv.index & 0xFF));
        }
    }
}

void BytecodeCompiler::compilePropertyAccess(const PropertyAccessExpr& expr) {
    compileExpr(expr.object);
    size_t nameIdx = identifierConstant(expr.property);
    
    size_t skipJump = 0;
    if (expr.isOptional) {
        emitOp(OpCode::DUP);
        skipJump = emitJump(OpCode::JUMP_IF_NIL);
    }
    
    emitOp(OpCode::LOAD_PROPERTY);
    emitBytes(static_cast<uint8_t>((nameIdx >> 8) & 0xFF),
              static_cast<uint8_t>(nameIdx & 0xFF));
    size_t icIdx = current->function->chunk.icEntries.size();
    current->function->chunk.icEntries.push_back(ICCacheEntry{});
    emitBytes(static_cast<uint8_t>((icIdx >> 8) & 0xFF), static_cast<uint8_t>(icIdx & 0xFF));
              
    if (expr.isOptional) {
        patchJump(skipJump);
    }
}

void BytecodeCompiler::compileSelf(const SelfExpr& /*expr*/) {
    emitLoadSelf();
}

void BytecodeCompiler::compileSuper(const SuperExpr& /*expr*/) {
    if (current->currentParentClass.empty()) {
        std::cerr << "Compile Error: Cannot use 'super' in a class with no parent." << std::endl;
        return;
    }
    
    // Push `self` (inst)
    emitLoadSelf();
    
    // Push parent class
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
}

void BytecodeCompiler::emitLoadSelf() {
    int local = resolveLocal("self");
    if (local != -1) {
        emitLoadLocal(local);
        return;
    }

    int upvalue = resolveUpvalue("self");
    if (upvalue != -1) {
        emitBytes(static_cast<uint8_t>(OpCode::LOAD_UPVALUE), static_cast<uint8_t>(upvalue));
        return;
    }

    errorAt("Cannot use 'self' or 'super' outside of a model method", currentLine);
}

void BytecodeCompiler::compileNew(const NewExpr& expr) {
    if (expr.arguments.size() > 255) {
        errorAt("Too many arguments to 'new' (max 255)", currentLine);
        return;
    }
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
    size_t nameIdx = identifierConstant(expr.name);

    if (expr.compoundOp.has_value()) {
        emitOp(OpCode::DUP);
        emitOp(OpCode::LOAD_PROPERTY);
        emitBytes(static_cast<uint8_t>((nameIdx >> 8) & 0xFF),
                  static_cast<uint8_t>(nameIdx & 0xFF));
        size_t icIdx = current->function->chunk.icEntries.size();
        current->function->chunk.icEntries.push_back(ICCacheEntry{});
        emitBytes(static_cast<uint8_t>((icIdx >> 8) & 0xFF), static_cast<uint8_t>(icIdx & 0xFF));
        compileExpr(expr.value);
        switch (expr.compoundOp.value()) {
            case TokenType::PLUS_EQUAL: emitOp(OpCode::ADD); break;
            case TokenType::MINUS_EQUAL: emitOp(OpCode::SUB); break;
            case TokenType::STAR_EQUAL: emitOp(OpCode::MUL); break;
            case TokenType::STAR_STAR_EQUAL: emitOp(OpCode::POW); break;
            case TokenType::SLASH_EQUAL: emitOp(OpCode::DIV); break;
            default: emitOp(OpCode::ADD); break;
        }
    } else {
        compileExpr(expr.value);
    }
    
    emitOp(OpCode::INTERCEPTED_STORE_PROPERTY);
    emitBytes(static_cast<uint8_t>((nameIdx >> 8) & 0xFF),
              static_cast<uint8_t>(nameIdx & 0xFF));
    size_t icIdx2 = current->function->chunk.icEntries.size();
    current->function->chunk.icEntries.push_back(ICCacheEntry{});
    emitBytes(static_cast<uint8_t>((icIdx2 >> 8) & 0xFF), static_cast<uint8_t>(icIdx2 & 0xFF));
}

void BytecodeCompiler::compileDictionary(const DictionaryExpr& expr) {
    for (const auto& [key, value] : expr.pairs) {
        compileExpr(key);
        compileExpr(value);
    }
    if (expr.pairs.size() > 255) {
        errorAt("Too many dictionary entries (max 255)", currentLine);
        return;
    }
    emitBytes(static_cast<uint8_t>(OpCode::MAKE_DICT),
              static_cast<uint8_t>(expr.pairs.size()));
}

void BytecodeCompiler::compileSpread(const SpreadExpr& expr) {
    errorAt("Spread expressions are only allowed inside arrays or function calls.", currentLine);
}

// ============================================================================
// Statement Compilation
// ============================================================================

