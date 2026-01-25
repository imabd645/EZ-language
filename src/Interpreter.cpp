#include "Interpreter.h"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <algorithm>
#include <sstream>
#include <fstream>
#include "Lexer.h"
#include "Parser.h"
#include "MiniJson.h"

Interpreter::Interpreter() {
    globalEnv = std::make_shared<Environment>();
    currentEnv = globalEnv;
    GarbageCollector::instance().setRoot(globalEnv);
    
    // Seed random number generator
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    
    initBuiltins();
}

#include "Builtins.h"

void Interpreter::initBuiltins() {
    registerBuiltins(*this);
}

void Interpreter::defineGlobal(const std::string& name, const Value& value) {
    globalEnv->define(name, value);
}

void Interpreter::interpret(const std::vector<StmtPtr>& statements) {
    try {
        for (const auto& stmt : statements) {
            execute(stmt);
        }
    } catch (const RuntimeError& e) {
        std::cerr << "[Line " << e.line << "] Runtime Error: " << e.what() << std::endl;
    } catch (const ReturnException&) {
        // Top-level return, just ignore
    }
}

Value Interpreter::evaluate(const ExprPtr& expr) {
    if (!expr) return Value();
    
    int line = expr->line;
    
    return std::visit([this, line](auto&& arg) -> Value {
        using T = std::decay_t<decltype(arg)>;
        
        if constexpr (std::is_same_v<T, std::shared_ptr<LiteralExpr>>) {
            return visitLiteral(arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<IdentifierExpr>>) {
            return visitIdentifier(arg, line);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<BinaryExpr>>) {
            return visitBinary(arg, line);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<UnaryExpr>>) {
            return visitUnary(arg, line);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<CallExpr>>) {
            return visitCall(arg, line);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<IndexExpr>>) {
            return visitIndex(arg, line);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<ArrayExpr>>) {
            return visitArray(arg, line);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<AssignExpr>>) {
            return visitAssign(arg, line);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<LogicalExpr>>) {
            return visitLogical(arg, line);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<LambdaExpr>>) {
            return visitLambda(arg, line);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<PropertyAccessExpr>>) {
            return visitPropertyAccess(arg, line);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<SelfExpr>>) {
            return visitSelf(arg, line);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<NewExpr>>) {
            return visitNew(arg, line);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<SetExpr>>) {
            return visitSet(arg, line);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<DictionaryExpr>>) {
            return visitDictionary(arg, line);
        }
        
        return Value();
    }, expr->variant);
}

void Interpreter::execute(const StmtPtr& stmt) {
    if (!stmt) return;
    
    std::visit([this](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        
        if constexpr (std::is_same_v<T, std::shared_ptr<ExprStmt>>) {
            visitExprStmt(arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<OutStmt>>) {
            visitOutStmt(arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<VarDeclStmt>>) {
            visitVarDeclStmt(arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<BlockStmt>>) {
            visitBlockStmt(arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<WhenStmt>>) {
            visitWhenStmt(arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<WhileStmt>>) {
            visitWhileStmt(arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<RepeatStmt>>) {
            visitRepeatStmt(arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<GetStmt>>) {
            visitGetStmt(arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<TaskStmt>>) {
            visitTaskStmt(arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<GiveStmt>>) {
            visitGiveStmt(arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<EscapeStmt>>) {
            visitEscapeStmt(arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<SkipStmt>>) {
            visitSkipStmt(arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<ModelStmt>>) {
            visitModelStmt(arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<StructStmt>>) {
            visitStructStmt(arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<UseStmt>>) {
            visitUseStmt(arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<TryStmt>>) {
            visitTryStmt(arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<ThrowStmt>>) {
            visitThrowStmt(arg);
        }
    }, stmt->variant);
}

// ============ Expression Visitors ============

Value Interpreter::visitLiteral(const std::shared_ptr<LiteralExpr>& expr) {
    return std::visit([](auto&& arg) -> Value {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            return Value();
        } else if constexpr (std::is_same_v<T, double>) {
            return Value(arg);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return Value(arg);
        } else if constexpr (std::is_same_v<T, bool>) {
            return Value(arg);
        }
        return Value();
    }, expr->value);
}

Value Interpreter::visitIdentifier(const std::shared_ptr<IdentifierExpr>& expr, int line) {
    return currentEnv->get(expr->name, line);
}

Value Interpreter::visitBinary(const std::shared_ptr<BinaryExpr>& expr, int line) {
    Value left = evaluate(expr->left);
    Value right = evaluate(expr->right);
    
    switch (expr->op) {
        case TokenType::PLUS:
            if (left.isNumber() && right.isNumber()) {
                return Value(left.asNumber() + right.asNumber());
            }
            if (left.isString() || right.isString()) {
                return Value(left.toString() + right.toString());
            }
            if (left.isArray() && right.isArray()) {
                auto result = left.asArray();
                const auto& rightArr = right.asArray();
                result.insert(result.end(), rightArr.begin(), rightArr.end());
                return Value::makeArray(result);
            }
            throw RuntimeError("Operands must be numbers, strings, or arrays for '+'", line);
            
        case TokenType::MINUS:
            checkNumberOperands(expr->op, left, right, line);
            return Value(left.asNumber() - right.asNumber());
            
        case TokenType::STAR:
            if (left.isNumber() && right.isNumber()) {
                return Value(left.asNumber() * right.asNumber());
            }
            if (left.isString() && right.isNumber()) {
                std::string result;
                int times = static_cast<int>(right.asNumber());
                for (int i = 0; i < times; i++) {
                    result += left.asString();
                }
                return Value(result);
            }
            throw RuntimeError("Operands must be numbers for '*'", line);
            
        case TokenType::SLASH:
            checkNumberOperands(expr->op, left, right, line);
            if (right.asNumber() == 0) {
                throw RuntimeError("Division by zero", line);
            }
            return Value(left.asNumber() / right.asNumber());
            
        case TokenType::PERCENT:
            checkNumberOperands(expr->op, left, right, line);
            return Value(std::fmod(left.asNumber(), right.asNumber()));
            
        case TokenType::EQUAL_EQUAL:
            return Value(left.equals(right));
            
        case TokenType::BANG_EQUAL:
            return Value(!left.equals(right));
            
        case TokenType::LESS:
            checkNumberOperands(expr->op, left, right, line);
            return Value(left.asNumber() < right.asNumber());
            
        case TokenType::LESS_EQUAL:
            checkNumberOperands(expr->op, left, right, line);
            return Value(left.asNumber() <= right.asNumber());
            
        case TokenType::GREATER:
            checkNumberOperands(expr->op, left, right, line);
            return Value(left.asNumber() > right.asNumber());
            
        case TokenType::GREATER_EQUAL:
            checkNumberOperands(expr->op, left, right, line);
            return Value(left.asNumber() >= right.asNumber());

        case TokenType::IN:
            if (right.isDictionary()) {
                return Value(right.asDictionary().map.count(left.toString()) > 0);
            }
            if (right.isArray()) {
                const auto& arr = right.asArray();
                for (const auto& elem : arr) {
                    if (left.equals(elem)) return Value(true);
                }
                return Value(false);
            }
            if (right.isString()) {
                return Value(right.asString().find(left.toString()) != std::string::npos);
            }
            throw RuntimeError("'in' operator expects dictionary, array, or string on right side", line);
            
        default:
            throw RuntimeError("Unknown binary operator", line);
    }
}

Value Interpreter::visitUnary(const std::shared_ptr<UnaryExpr>& expr, int line) {
    Value operand = evaluate(expr->operand);
    
    switch (expr->op) {
        case TokenType::MINUS:
            checkNumberOperand(expr->op, operand, line);
            return Value(-operand.asNumber());
            
        case TokenType::BANG:
        case TokenType::NOT:
            return Value(!operand.isTruthy());
            
        default:
            throw RuntimeError("Unknown unary operator", line);
    }
}

Value Interpreter::visitCall(const std::shared_ptr<CallExpr>& expr, int line) {
    Value callee = evaluate(expr->callee);
    
    std::vector<Value> args;
    for (const auto& arg : expr->arguments) {
        args.push_back(evaluate(arg));
    }
    
    return callFunction(callee, args, line);
}

Value Interpreter::visitIndex(const std::shared_ptr<IndexExpr>& expr, int line) {
    Value object = evaluate(expr->object);
    Value index = evaluate(expr->index);
    
    if (object.isArray()) {
        if (!index.isNumber()) {
            throw RuntimeError("Array index must be a number", line);
        }
        int idx = static_cast<int>(index.asNumber());
        const auto& arr = object.asArray();
        if (idx < 0 || idx >= static_cast<int>(arr.size())) {
            throw RuntimeError("Array index out of bounds: " + std::to_string(idx), line);
        }
        return arr[idx];
    }
    
    if (object.isString()) {
        if (!index.isNumber()) {
            throw RuntimeError("String index must be a number", line);
        }
        int idx = static_cast<int>(index.asNumber());
        const auto& str = object.asString();
        if (idx < 0 || idx >= static_cast<int>(str.length())) {
            throw RuntimeError("String index out of bounds: " + std::to_string(idx), line);
        }
        return Value(std::string(1, str[idx]));
    }
    
    if (object.isDictionary()) {
        std::string key = index.toString();
        const auto& dict = object.asDictionary();
        auto it = dict.map.find(key);
        if (it != dict.map.end()) {
            return it->second;
        }
        return Value(); // nil if key not found
    }
    
    throw RuntimeError("Can only index arrays, strings, or dictionaries", line);
}

Value Interpreter::visitArray(const std::shared_ptr<ArrayExpr>& expr, int line) {
    std::vector<Value> elements;
    for (const auto& elem : expr->elements) {
        elements.push_back(evaluate(elem));
    }
    return Value::makeArray(elements);
}

Value Interpreter::visitAssign(const std::shared_ptr<AssignExpr>& expr, int line) {
    Value value = evaluate(expr->value);
    
    if (expr->index) {
        // Indexed assignment (obj[idx] = val)
        
        if (expr->object) {
            // Complex object like obj.prop[idx] = val
            Value object = evaluate(expr->object);
            if (object.isArray()) {
                Value indexVal = evaluate(expr->index);
                if (!indexVal.isNumber()) throw RuntimeError("Array index must be a number", line);
                int idx = static_cast<int>(indexVal.asNumber());
                auto& arr = object.asArray();
                if (idx < 0 || idx >= static_cast<int>(arr.size())) throw RuntimeError("Array index out of bounds", line);
                arr[idx] = value;
            } else if (object.isDictionary()) {
                Value indexVal = evaluate(expr->index);
                object.asDictionary().map[indexVal.toString()] = value;
            } else {
                throw RuntimeError("Target of indexed assignment must be array or dictionary", line);
            }
        } else {
            // Simple variable indexed assignment: arr[idx] = val
            Value* targetPtr = currentEnv->getPtr(expr->name);
            if (!targetPtr) throw RuntimeError("Undefined variable '" + expr->name + "'", line);
            
            Value indexVal = evaluate(expr->index);
            if (targetPtr->isArray()) {
                if (!indexVal.isNumber()) throw RuntimeError("Array index must be a number", line);
                int idx = static_cast<int>(indexVal.asNumber());
                auto& arr = targetPtr->asArray();
                if (idx < 0 || idx >= static_cast<int>(arr.size())) throw RuntimeError("Array index out of bounds", line);
                arr[idx] = value;
            } else if (targetPtr->isDictionary()) {
                targetPtr->asDictionary().map[indexVal.toString()] = value;
            } else {
                throw RuntimeError("Only arrays and dictionaries can be indexed", line);
            }
        }
        return value;
    } else {
        // Simple assignment
        currentEnv->assign(expr->name, value, line);
    }
    
    return value;
}

Value Interpreter::visitLogical(const std::shared_ptr<LogicalExpr>& expr, int line) {
    Value left = evaluate(expr->left);
    
    if (expr->op == TokenType::OR) {
        if (left.isTruthy()) return left;
    } else { // AND
        if (!left.isTruthy()) return left;
    }
    
    return evaluate(expr->right);
}

Value Interpreter::visitLambda(const std::shared_ptr<LambdaExpr>& expr, int line) {
    // Capture current environment for closure
    auto closure = currentEnv;
    
    if (expr->body) {
        // Expression body lambda - wrap in a give statement
        std::vector<StmtPtr> body;
        body.push_back(makeGiveStmt(line, expr->body));
        return Value::makeFunction("<lambda>", expr->params, body, closure);
    } else {
        // Statement body lambda
        return Value::makeFunction("<lambda>", expr->params, expr->stmtBody, closure);
    }
}

// ============ Statement Visitors ============

void Interpreter::visitExprStmt(const std::shared_ptr<ExprStmt>& stmt) {
    evaluate(stmt->expression);
}

void Interpreter::visitOutStmt(const std::shared_ptr<OutStmt>& stmt) {
    Value value = evaluate(stmt->expression);
    std::cout << value.toString() << std::endl;
}

void Interpreter::visitVarDeclStmt(const std::shared_ptr<VarDeclStmt>& stmt) {
    Value value = evaluate(stmt->initializer);
    // Use assign if variable already exists (to update existing variable)
    // Otherwise define new variable
    if (currentEnv->contains(stmt->name)) {
        currentEnv->assign(stmt->name, value, 0);
    } else {
        currentEnv->define(stmt->name, value);
    }
}

void Interpreter::visitBlockStmt(const std::shared_ptr<BlockStmt>& stmt) {
    executeBlock(stmt->statements, currentEnv->createChild());
}

void Interpreter::visitWhenStmt(const std::shared_ptr<WhenStmt>& stmt) {
    Value condition = evaluate(stmt->condition);
    
    if (condition.isTruthy()) {
        execute(stmt->thenBranch);
    } else if (stmt->elseBranch) {
        execute(stmt->elseBranch);
    }
}

void Interpreter::visitWhileStmt(const std::shared_ptr<WhileStmt>& stmt) {
    while (evaluate(stmt->condition).isTruthy()) {
        try {
            execute(stmt->body);
        } catch (const BreakException&) {
            break;
        } catch (const ContinueException&) {
            continue;
        }
    }
}

void Interpreter::visitRepeatStmt(const std::shared_ptr<RepeatStmt>& stmt) {
    Value startVal = evaluate(stmt->start);
    Value endVal = evaluate(stmt->end);
    
    if (!startVal.isNumber() || !endVal.isNumber()) {
        throw RuntimeError("Repeat bounds must be numbers", 0);
    }
    
    int start = static_cast<int>(startVal.asNumber());
    int end = static_cast<int>(endVal.asNumber());
    
    auto loopEnv = currentEnv->createChild();
    auto prevEnv = currentEnv;
    currentEnv = loopEnv;
    
    // Support both upward and downward loops
    if (start <= end) {
        for (int i = start; i <= end; i++) {
            loopEnv->define(stmt->variable, Value(static_cast<double>(i)));
            try {
                execute(stmt->body);
            } catch (const BreakException&) {
                break;
            } catch (const ContinueException&) {
                continue;
            }
        }
    } else {
        for (int i = start; i >= end; i--) {
            loopEnv->define(stmt->variable, Value(static_cast<double>(i)));
            try {
                execute(stmt->body);
            } catch (const BreakException&) {
                break;
            } catch (const ContinueException&) {
                continue;
            }
        }
    }
    
    currentEnv = prevEnv;
}

void Interpreter::visitGetStmt(const std::shared_ptr<GetStmt>& stmt) {
    Value iterable = evaluate(stmt->iterable);
    
    if (!iterable.isArray() && !iterable.isString() && !iterable.isDictionary()) {
        throw RuntimeError("Can only iterate over arrays, strings, and dictionaries", 0);
    }
    
    auto loopEnv = currentEnv->createChild();
    auto prevEnv = currentEnv;
    currentEnv = loopEnv;
    
    if (iterable.isArray()) {
        for (const auto& elem : iterable.asArray()) {
            loopEnv->define(stmt->variable, elem);
            
            try {
                execute(stmt->body);
            } catch (const BreakException&) {
                break;
            } catch (const ContinueException&) {
                continue;
            }
        }
    } else if (iterable.isDictionary()) {
        const auto& map = iterable.asDictionary().map;
        std::vector<std::string> keys;
        for (const auto& pair : map) keys.push_back(pair.first);
        
        for (const auto& key : keys) {
            loopEnv->define(stmt->variable, Value(key));
            try {
                execute(stmt->body);
            } catch (const BreakException&) {
                break;
            } catch (const ContinueException&) {
                continue;
            }
        }
    } else {
        const std::string& str = iterable.asString();
        for (char c : str) {
            loopEnv->define(stmt->variable, Value(std::string(1, c)));
            
            try {
                execute(stmt->body);
            } catch (const BreakException&) {
                break;
            } catch (const ContinueException&) {
                continue;
            }
        }
    }
    
    currentEnv = prevEnv;
}

void Interpreter::visitTaskStmt(const std::shared_ptr<TaskStmt>& stmt) {
    Value function = Value::makeFunction(stmt->name, stmt->params, stmt->body, currentEnv);
    currentEnv->define(stmt->name, function);
}

void Interpreter::visitGiveStmt(const std::shared_ptr<GiveStmt>& stmt) {
    Value value;
    if (stmt->value) {
        value = evaluate(stmt->value);
    }
    throw ReturnException(value);
}

void Interpreter::visitEscapeStmt(const std::shared_ptr<EscapeStmt>&) {
    throw BreakException();
}

void Interpreter::visitSkipStmt(const std::shared_ptr<SkipStmt>&) {
    throw ContinueException();
}

// ============ Helpers ============

void Interpreter::executeBlock(const std::vector<StmtPtr>& statements, std::shared_ptr<Environment> env) {
    auto prevEnv = currentEnv;
    currentEnv = env;
    
    try {
        for (const auto& stmt : statements) {
            execute(stmt);
        }
    } catch (...) {
        currentEnv = prevEnv;
        throw;
    }
    
    currentEnv = prevEnv;
}

Value Interpreter::callFunction(const Value& callee, const std::vector<Value>& args, int line) {
    if (callee.isNativeFunction()) {
        auto nativeFn = callee.asNativeFunction();
        if (nativeFn->arity != -1 && static_cast<int>(args.size()) != nativeFn->arity) {
            throw RuntimeError("Expected " + std::to_string(nativeFn->arity) + 
                             " arguments but got " + std::to_string(args.size()), line);
        }
        return nativeFn->function(*this, args);
    }
    
    if (callee.isFunction()) {
        auto func = callee.asFunction();
        if (args.size() != func->params.size()) {
            throw RuntimeError("Expected " + std::to_string(func->params.size()) + 
                             " arguments but got " + std::to_string(args.size()), line);
        }
        
        auto funcEnv = std::make_shared<Environment>(func->closure);
        for (size_t i = 0; i < func->params.size(); i++) {
            funcEnv->define(func->params[i], args[i]);
        }
        
        try {
            executeBlock(func->body, funcEnv);
        } catch (const ReturnException& e) {
            return e.value;
        }
        
        return Value();
    }
    
    if (callee.isClass()) {
        auto klass = callee.asClass();
        auto instance = std::make_shared<EZInstance>(klass);
        Value instanceVal(instance);
        
        // Check argument count match for init
        if (args.size() != klass->initParams.size()) {
            throw RuntimeError("Expected " + std::to_string(klass->initParams.size()) + 
                               " arguments for init but got " + std::to_string(args.size()), line);
        }
        
        // Run init method if present
        if (!klass->initBody.empty()) {
            auto methodEnv = globalEnv->createChild(); 
            methodEnv->define("self", instanceVal);
            
            // Define params
            for (size_t i = 0; i < args.size(); i++) {
                methodEnv->define(klass->initParams[i], args[i]);
            }
            
            // Execute init body
            std::shared_ptr<Environment> previousEnv = currentEnv;
            currentEnv = methodEnv;
            
            try {
                executeBlock(klass->initBody, methodEnv);
            } catch (const ReturnException&) {
                // init ignored return
            } catch (...) {
                currentEnv = previousEnv;
                throw;
            }
            
            currentEnv = previousEnv;
        }
        
        return instanceVal;
    }
    
    throw RuntimeError("Can only call functions or models", line);
}

void Interpreter::checkNumberOperand(TokenType op, const Value& operand, int line) {
    if (!operand.isNumber()) {
        throw RuntimeError("Operand must be a number", line);
    }
}

void Interpreter::checkNumberOperands(TokenType op, const Value& left, const Value& right, int line) {
    if (!left.isNumber() || !right.isNumber()) {
        throw RuntimeError("Operands must be numbers", line);
    }
}

// ============ OOP Visitors ============

Value Interpreter::visitSelf(const std::shared_ptr<SelfExpr>& expr, int line) {
    return currentEnv->get("self", line);
}

Value Interpreter::visitNew(const std::shared_ptr<NewExpr>& expr, int line) {
    Value classVal = globalEnv->get(expr->className, line);
    if (!classVal.isClass()) {
        throw RuntimeError("'" + expr->className + "' is not a model", line);
    }
    
    auto klass = classVal.asClass();
    auto instance = std::make_shared<EZInstance>(klass);
    Value instanceVal(instance);
    
    // Evaluate arguments
    std::vector<Value> args;
    for (const auto& arg : expr->arguments) {
        args.push_back(evaluate(arg));
    }
    
    // Check argument count match for init
    if (args.size() != klass->initParams.size()) {
        throw RuntimeError("Expected " + std::to_string(klass->initParams.size()) + 
                           " arguments for init but got " + std::to_string(args.size()), line);
    }
    
    // Run init method if present
    if (!klass->initBody.empty()) {
        auto initEnv = std::make_shared<Environment>(currentEnv); 
        
        // Create a method scope with self
        // Note: we need to link it properly. 
        // For init, we can just create a child of global (where class sits) or current (instantiation site)?
        // Init should have access to global scope.
        auto methodEnv = globalEnv->createChild(); 
        methodEnv->define("self", instanceVal);
        
        // Define params
        for (size_t i = 0; i < args.size(); i++) {
            methodEnv->define(klass->initParams[i], args[i]);
        }
        
        // Execute init body
        std::shared_ptr<Environment> previousEnv = currentEnv;
        currentEnv = methodEnv;
        
        try {
            executeBlock(klass->initBody, methodEnv);
        } catch (const ReturnException&) {
            // init ignored return
        } catch (...) {
            currentEnv = previousEnv;
            throw;
        }
        
        currentEnv = previousEnv;
    }
    
    return instanceVal;
}

Value Interpreter::visitPropertyAccess(const std::shared_ptr<PropertyAccessExpr>& expr, int line) {
    Value object = evaluate(expr->object);
    
    // Get property
    if (object.isInstance()) {
        auto instance = object.asInstance();
        
        // Check visibility first (walk up class hierarchy)
        auto searchKlass = instance->klass;
        while (searchKlass) {
            if (searchKlass->visibility.find(expr->property) != searchKlass->visibility.end()) {
                bool isPublic = searchKlass->visibility[expr->property];
                if (!isPublic) {
                    // Private member
                    try {
                        if (currentEnv->contains("self")) {
                            Value self = currentEnv->get("self");
                            if (!self.isInstance() || self.asInstance() != instance) {
                                throw RuntimeError("Cannot access hidden member '" + expr->property + "'", line);
                            }
                        } else {
                            throw RuntimeError("Cannot access hidden member '" + expr->property + "'", line);
                        }
                    } catch (const RuntimeError&) {
                        throw RuntimeError("Cannot access hidden member '" + expr->property + "'", line);
                    }
                }
                break; // Found declaration, visibility checked
            }
            searchKlass = searchKlass->parent;
        }
        
        // 1. Check instance properties
        if (instance->hasProperty(expr->property)) {
            return instance->getProperty(expr->property);
        }
        
        // 2. Check class methods
        auto klass = instance->klass;
        while (klass) {
            auto it = klass->methods.find(expr->property);
            if (it != klass->methods.end()) {
                // Method found (visibility already checked above)
                Value method = it->second;
                if (!method.isFunction()) return method;
                
                // Bind 'self' to the method
                auto func = method.asFunction();
                auto boundEnv = func->closure->createChild();
                boundEnv->define("self", object);
                return Value::makeFunction(func->name, func->params, func->body, boundEnv);
            }
            klass = klass->parent;
        }
        
        throw RuntimeError("Undefined property '" + expr->property + "'", line);
    } else if (object.isArray() && expr->property == "len") {
        return Value(static_cast<double>(object.asArray().size()));
    } else if (object.isString() && expr->property == "len") {
        return Value(static_cast<double>(object.asString().length()));
    } else if (object.isDictionary()) {
        auto& map = object.asDictionary().map;
        auto it = map.find(expr->property);
        if (it != map.end()) return it->second;
        return Value(); // nil if not found
    }
    
    throw RuntimeError("Only objects have properties", line);
}

void Interpreter::visitModelStmt(const std::shared_ptr<ModelStmt>& stmt) {
    auto klass = std::make_shared<EZClass>(stmt->name);
    
    // Handle inheritance
    if (!stmt->parentName.empty()) {
        Value parentVal = globalEnv->get(stmt->parentName, stmt->line);
        if (!parentVal.isClass()) {
            throw RuntimeError("Parent '" + stmt->parentName + "' must be a model", stmt->line);
        }
        klass->parent = parentVal.asClass();
    }
    
    // Init
    klass->initParams = stmt->initParams;
    klass->initBody = stmt->initBody;
    
    // Process members
    for (const auto& member : stmt->members) {
        bool isPublic = (member.visibility == MemberVisibility::PUBLIC);
        klass->visibility[member.name] = isPublic;
        
        if (member.isMethod) {
            // Method - capture global env as closure (methods are shared)
            Value method = Value::makeFunction(
                member.name, member.params, member.body, globalEnv
            );
            klass->methods[member.name] = method;
        } else {
            // Properties are dynamic, but we can store visibility
            // We could process initializers here if we had a way to store them in the class
            // For now, assume properties are initialized in init() via self.prop = val
        }
    }
    
    globalEnv->define(stmt->name, Value(klass));
}

Value Interpreter::visitSet(const std::shared_ptr<SetExpr>& expr, int line) {
    Value object = evaluate(expr->object);
    
    if (!object.isInstance() && !object.isDictionary()) {
        throw RuntimeError("Only instances or dictionaries have fields", line);
    }
    
    Value value = evaluate(expr->value);
    
    if (object.isDictionary()) {
        object.asDictionary().map[expr->name] = value;
        return value;
    }
    
    auto instance = object.asInstance();
    
    // Check if we can set this property (visibility check)
    // Check if the property is declared as hidden in the class or parent classes
    auto klass = instance->klass;
    while (klass) {
        if (klass->visibility.find(expr->name) != klass->visibility.end()) {
            bool isPublic = klass->visibility[expr->name];
            if (!isPublic) {
                 // Check if 'self' refers to this instance
                 try {
                     if (currentEnv->contains("self")) {
                        Value self = currentEnv->get("self");
                        if (!self.isInstance() || self.asInstance() != instance) {
                            throw RuntimeError("Cannot modify hidden member '" + expr->name + "'", line);
                        }
                     } else {
                         throw RuntimeError("Cannot modify hidden member '" + expr->name + "'", line);
                     }
                 } catch (const RuntimeError&) {
                     throw RuntimeError("Cannot modify hidden member '" + expr->name + "'", line);
                 }
            }
            break; // Found declaration, stop checking parents
        }
        klass = klass->parent;
    }
    
    instance->setProperty(expr->name, value);
    return value;
}

Value Interpreter::visitDictionary(const std::shared_ptr<DictionaryExpr>& expr, int line) {
    auto dict = Value::makeDictionary();
    auto& map = dict.asDictionary().map;
    
    for (const auto& pair : expr->pairs) {
        Value key = evaluate(pair.first);
        Value val = evaluate(pair.second);
        map[key.toString()] = val;
    }
    return dict;
}

void Interpreter::visitStructStmt(const std::shared_ptr<StructStmt>& stmt) {
    // Treat struct as a class with auto-generated init method
    auto klass = std::make_shared<EZClass>(stmt->name);
    
    // Init method params are the fields
    klass->initParams = stmt->fields;
    
    // Synthesize body for init: self.field = field
    int line = 0; // generated logic
    for (const auto& field : stmt->fields) {
        // self
        auto selfExpr = makeSelfExpr(line);
        // value (param)
        auto valExpr = makeIdentifierExpr(line, field);
        // self.field = value
        auto setExpr = makeSetExpr(line, selfExpr, field, valExpr);
        // Stmt
        klass->initBody.push_back(makeExprStmt(line, setExpr));
    }
    
    defineGlobal(stmt->name, Value(klass));
}

void Interpreter::visitUseStmt(const std::shared_ptr<UseStmt>& stmt) {
    std::string path = stmt->path;
    std::ifstream file(path);
    
    // Check local file
    if (!file.is_open()) {
        // Check C:/ezlib
        std::string libPath = "C:/ezlib/" + path;
        file.open(libPath);
        
        if (file.is_open()) {
            path = libPath;
        } else {
             // Check package.ez
             std::string pkgEzPath = "C:/ezlib/" + path + "/package.ez";
             std::ifstream pkgFile(pkgEzPath);
             bool found = false;
             
             if (pkgFile.is_open()) {
                 MiniJson::Value root;
                 MiniJson::Reader reader;
                 if (reader.parse(pkgFile, root)) {
                     std::string mainFile = root.get("main", "main.ez").asString();
                     std::string mainPath = "C:/ezlib/" + path + "/" + mainFile;
                     file.open(mainPath);
                     if (file.is_open()) {
                         path = mainPath;
                         found = true;
                     }
                 }
             }
             
             if (!found) {
                 // Try .ez
                 std::string ezPath = "C:/ezlib/" + path + ".ez";
                 file.open(ezPath);
                 if (file.is_open()) {
                     path = ezPath;
                 } else {
                     // Try default main.ez
                     std::string defPath = "C:/ezlib/" + path + "/main.ez";
                     file.open(defPath);
                     if (file.is_open()) {
                         path = defPath;
                     } else {
                         throw RuntimeError("Could not find module '" + path + "'", 0);
                     }
                 }
             }
        }
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    if (lexer.hasError()) {
        throw RuntimeError("Lexer error in module '" + path + "'", 0);
    }
    
    Parser parser(tokens);
    auto statements = parser.parse();
    if (parser.hasError()) {
         throw RuntimeError("Parser error in module '" + path + "'", 0);
    }
    
    for (const auto& s : statements) {
        execute(s);
    }
}

void Interpreter::visitTryStmt(const std::shared_ptr<TryStmt>& stmt) {
    try {
        execute(stmt->tryBlock);
    } catch (const RuntimeError& e) {
        auto catchEnv = currentEnv->createChild();
        catchEnv->define(stmt->catchVar, Value(std::string(e.what())));
        
        auto prevEnv = currentEnv;
        currentEnv = catchEnv;
        
        try {
            execute(stmt->catchBlock);
        } catch (...) {
            currentEnv = prevEnv;
            throw;
        }
        currentEnv = prevEnv;
    }
}

void Interpreter::visitThrowStmt(const std::shared_ptr<ThrowStmt>& stmt) {
    Value val = evaluate(stmt->expression);
    throw RuntimeError(val.toString(), stmt->expression->line);
}
