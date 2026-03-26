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

Interpreter::Interpreter(std::shared_ptr<Environment> startEnv) {
    globalEnv = startEnv;
    currentEnv = globalEnv; // Start execution in this environment
    GarbageCollector::instance().setRoot(globalEnv);
    // Skip initBuiltins() and srand()
}

#include "Builtins.h"
#include "GUIBuiltins.h"

void Interpreter::initBuiltins() {
    registerBuiltins(*this);
    registerGUIBuiltins(*this);
}

void Interpreter::defineGlobal(const std::string& name, const Value& value) {
    globalEnv->define(name, value);
}

void Interpreter::runtimeError(const std::string& message, int line, const std::string& filename) {
    std::cerr << "Runtime Error: " << message << std::endl;
    std::cerr << "  at [line " << line << "] in " << (filename.empty() ? "main" : filename) << std::endl;
    
    // Print stack trace
    for (auto it = callStack.rbegin(); it != callStack.rend(); ++it) {
        std::cerr << "  at " << it->functionName << "() in " 
                  << (it->filename.empty() ? "main" : it->filename) 
                  << ":" << it->line << std::endl;
    }
    
    throw RuntimeError(message, line);
}

void Interpreter::interpret(const std::vector<StmtPtr>& statements) {
    try {
        for (const auto& stmt : statements) {
            execute(stmt);
        }
    } catch (const RuntimeError&) {
        // Error already printed by runtimeError()
    } catch (const ReturnException&) {
        // Top-level return, just ignore
    }
}

Value Interpreter::evaluate(const ExprPtr& expr) {
    if (!expr) return Value();
    
    int line = expr->line;
    const std::string& filename = expr->filename;
    
    return std::visit([this, line, &filename](auto&& arg) -> Value {
        using T = std::decay_t<decltype(arg)>;
        
        if constexpr (std::is_same_v<T, std::shared_ptr<LiteralExpr>>) {
            return visitLiteral(arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<IdentifierExpr>>) {
            return visitIdentifier(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<BinaryExpr>>) {
            return visitBinary(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<UnaryExpr>>) {
            return visitUnary(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<CallExpr>>) {
            return visitCall(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<IndexExpr>>) {
            return visitIndex(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<ArrayExpr>>) {
            return visitArray(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<AssignExpr>>) {
            return visitAssign(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<LogicalExpr>>) {
            return visitLogical(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<LambdaExpr>>) {
            return visitLambda(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<PropertyAccessExpr>>) {
            return visitPropertyAccess(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<SelfExpr>>) {
            return visitSelf(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<NewExpr>>) {
            return visitNew(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<SetExpr>>) {
            return visitSet(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<DictionaryExpr>>) {
            return visitDictionary(arg, line, filename);
        }
        
        return Value();
    }, expr->variant);
}

void Interpreter::execute(const StmtPtr& stmt) {
    if (!stmt) return;
    
    int line = stmt->line;
    const std::string& filename = stmt->filename;
    
    std::visit([this, line, &filename](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        
        if constexpr (std::is_same_v<T, std::shared_ptr<ExprStmt>>) {
            visitExprStmt(arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<OutStmt>>) {
            visitOutStmt(arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<VarDeclStmt>>) {
            visitVarDeclStmt(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<BlockStmt>>) {
            visitBlockStmt(arg);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<WhenStmt>>) {
            visitWhenStmt(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<WhileStmt>>) {
            visitWhileStmt(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<RepeatStmt>>) {
            visitRepeatStmt(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<GetStmt>>) {
            visitGetStmt(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<TaskStmt>>) {
            visitTaskStmt(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<GiveStmt>>) {
            visitGiveStmt(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<EscapeStmt>>) {
            visitEscapeStmt(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<SkipStmt>>) {
            visitSkipStmt(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<ModelStmt>>) {
            visitModelStmt(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<StructStmt>>) {
            visitStructStmt(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<UseStmt>>) {
            visitUseStmt(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<TryStmt>>) {
            visitTryStmt(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<ThrowStmt>>) {
            visitThrowStmt(arg, line, filename);
        } else if constexpr (std::is_same_v<T, std::shared_ptr<InterfaceStmt>>) {
            visitInterfaceStmt(arg, line, filename);
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

Value Interpreter::visitIdentifier(const std::shared_ptr<IdentifierExpr>& expr, int line, const std::string& filename) {
    auto value = currentEnv->get(expr->name, line);
    if (value.isNil() && !currentEnv->contains(expr->name)) {
        runtimeError("Undefined variable '" + expr->name + "'", line, filename);
    }
    return value;
}

Value Interpreter::visitBinary(const std::shared_ptr<BinaryExpr>& expr, int line, const std::string& filename) {
    Value left = evaluate(expr->left);
    Value right = evaluate(expr->right);
    
    switch (expr->op) {
        case TokenType::PLUS:
            if (left.isNumber() && right.isNumber()) return left.asNumber() + right.asNumber();
            if (left.isString() || right.isString()) return stringify(left, line) + stringify(right, line);
            if (left.isArray() && right.isArray()) {
                auto newArr = std::make_shared<Value::ArrayType>(left.asArray());
                newArr->insert(newArr->end(), right.asArray().begin(), right.asArray().end());
                return Value(newArr);
            }
            runtimeError("Operands must be numbers, strings or arrays", line, filename);
            break;
        case TokenType::MINUS:
            checkNumberOperands(expr->op, left, right, line, filename);
            return left.asNumber() - right.asNumber();
        case TokenType::STAR:
            if (left.isNumber() && right.isNumber()) return left.asNumber() * right.asNumber();
            if (left.isString() && right.isNumber()) {
                std::string res = "";
                for (int i = 0; i < (int)right.asNumber(); i++) res += left.asString();
                return res;
            }
            runtimeError("Operands must be numbers or string * number", line, filename);
            break;
        case TokenType::SLASH:
            checkNumberOperands(expr->op, left, right, line, filename);
            if (right.asNumber() == 0) runtimeError("Division by zero", line, filename);
            return left.asNumber() / right.asNumber();
        case TokenType::PERCENT:
            checkNumberOperands(expr->op, left, right, line, filename);
            if (right.asNumber() == 0) runtimeError("Modulo by zero", line, filename);
            return std::fmod(left.asNumber(), right.asNumber());
            
        case TokenType::EQUAL_EQUAL:
            return Value(left.equals(right));
            
        case TokenType::BANG_EQUAL:
            return Value(!left.equals(right));
            
        case TokenType::LESS:
            checkNumberOperands(expr->op, left, right, line, filename);
            return left.asNumber() < right.asNumber();
        case TokenType::LESS_EQUAL:
            checkNumberOperands(expr->op, left, right, line, filename);
            return left.asNumber() <= right.asNumber();
        case TokenType::GREATER:
            checkNumberOperands(expr->op, left, right, line, filename);
            return left.asNumber() > right.asNumber();
        case TokenType::GREATER_EQUAL:
            checkNumberOperands(expr->op, left, right, line, filename);
            return left.asNumber() >= right.asNumber();

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
            runtimeError("'in' operator expects dictionary, array, or string on right side", line, filename);
            return Value();

        default:
            runtimeError("Unknown binary operator", line, filename);
            return Value();
    }
    return Value();
}

Value Interpreter::visitUnary(const std::shared_ptr<UnaryExpr>& expr, int line, const std::string& filename) {
    Value operand = evaluate(expr->operand);
    
    switch (expr->op) {
        case TokenType::MINUS:
            checkNumberOperand(expr->op, operand, line, filename);
            return Value(-operand.asNumber());
            
        case TokenType::BANG:
        case TokenType::NOT:
            return Value(!operand.isTruthy());
            
        default:
            runtimeError("Unknown unary operator", line, filename);
            return Value(); // Should not be reached
    }
}

Value Interpreter::visitCall(const std::shared_ptr<CallExpr>& expr, int line, const std::string& filename) {
    Value callee = evaluate(expr->callee);
    
    std::vector<Value> arguments;
    for (const auto& arg : expr->arguments) {
        arguments.push_back(evaluate(arg));
    }
    
    return callFunction(callee, arguments, line, filename);
}

Value Interpreter::visitIndex(const std::shared_ptr<IndexExpr>& expr, int line, const std::string& filename) {
    Value object = evaluate(expr->object);
    Value index = evaluate(expr->index);
    
    if (object.isArray()) {
        if (!index.isNumber()) {
            runtimeError("Array index must be a number", line, filename);
        }
        int idx = static_cast<int>(index.asNumber());
        const auto& arr = object.asArray();
        if (idx < 0 || idx >= static_cast<int>(arr.size())) {
            runtimeError("Array index out of bounds: " + std::to_string(idx), line, filename);
        }
        return arr[idx];
    }
    
    if (object.isString()) {
        if (!index.isNumber()) {
            runtimeError("String index must be a number", line, filename);
        }
        int idx = static_cast<int>(index.asNumber());
        const auto& str = object.asString();
        if (idx < 0 || idx >= static_cast<int>(str.length())) {
            runtimeError("String index out of bounds: " + std::to_string(idx), line, filename);
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
    
    runtimeError("Can only index arrays, strings, or dictionaries", line, filename);
    return Value(); // Should not be reached
}

Value Interpreter::visitArray(const std::shared_ptr<ArrayExpr>& expr, int line, const std::string& filename) {
    std::vector<Value> elements;
    for (const auto& elem : expr->elements) {
        elements.push_back(evaluate(elem));
    }
    return Value::makeArray(elements);
}

Value Interpreter::visitAssign(const std::shared_ptr<AssignExpr>& expr, int line, const std::string& filename) {
    Value value = evaluate(expr->value);
    
    if (expr->index) {
        // Indexed assignment (obj[idx] = val)
        
        if (expr->object) {
            // Complex object like obj.prop[idx] = val
            Value object = evaluate(expr->object);
            if (object.isArray()) {
                Value indexVal = evaluate(expr->index);
                if (!indexVal.isNumber()) runtimeError("Array index must be a number", line, filename);
                int idx = static_cast<int>(indexVal.asNumber());
                auto& arr = object.asArray();
                if (idx < 0 || idx >= static_cast<int>(arr.size())) runtimeError("Array index out of bounds", line, filename);
                arr[idx] = value;
            } else if (object.isDictionary()) {
                Value indexVal = evaluate(expr->index);
                object.asDictionary().map[indexVal.toString()] = value;
            } else {
                runtimeError("Target of indexed assignment must be array or dictionary", line, filename);
            }
        } else {
            // Simple variable indexed assignment: arr[idx] = val
            Value* targetPtr = currentEnv->getPtr(expr->name);
            if (!targetPtr) runtimeError("Undefined variable '" + expr->name + "'", line, filename);
            
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
                runtimeError("Only arrays and dictionaries can be indexed", line, filename);
            }
        }
        return value;
    } else {
        // Simple assignment
        currentEnv->assign(expr->name, value, line);
    }
    
    return value;
}

Value Interpreter::visitLogical(const std::shared_ptr<LogicalExpr>& expr, int line, const std::string& filename) {
    Value left = evaluate(expr->left);
    
    if (expr->op == TokenType::OR) {
        if (left.isTruthy()) return left;
    } else { // AND
        if (!left.isTruthy()) return left;
    }
    
    return evaluate(expr->right);
}

Value Interpreter::visitLambda(const std::shared_ptr<LambdaExpr>& expr, int line, const std::string& filename) {
    // Capture current environment for closure
    auto closure = currentEnv;
    
    if (expr->body) {
        // Expression body lambda - wrap in a give statement
        std::vector<StmtPtr> body;
        body.push_back(makeGiveStmt(line, filename, expr->body));
        return Value::makeFunction("<lambda>", expr->params, std::vector<ExprPtr>{}, body, closure);
    } else {
        // Statement body lambda
        return Value::makeFunction("<lambda>", expr->params, std::vector<ExprPtr>{}, expr->stmtBody, closure);
    }
}

// ============ Statement Visitors ============

void Interpreter::visitExprStmt(const std::shared_ptr<ExprStmt>& stmt) {
    evaluate(stmt->expression);
}

void Interpreter::visitOutStmt(const std::shared_ptr<OutStmt>& stmt) {
    Value value = evaluate(stmt->expression);
    std::cout << stringify(value, stmt->expression->line) << std::endl;
}

void Interpreter::visitVarDeclStmt(const std::shared_ptr<VarDeclStmt>& stmt, int line, const std::string& filename) {
    Value value = evaluate(stmt->initializer);
    // Use assign if variable already exists (to update existing variable)
    // Otherwise define new variable
    if (currentEnv->contains(stmt->name)) {
        currentEnv->assign(stmt->name, value, line);
    } else {
        currentEnv->define(stmt->name, value);
    }
}

void Interpreter::visitBlockStmt(const std::shared_ptr<BlockStmt>& stmt) {
    executeBlock(stmt->statements, currentEnv->createChild());
}

void Interpreter::visitWhenStmt(const std::shared_ptr<WhenStmt>& stmt, int line, const std::string& filename) {
    Value condition = evaluate(stmt->condition);
    
    if (condition.isTruthy()) {
        execute(stmt->thenBranch);
    } else if (stmt->elseBranch) {
        execute(stmt->elseBranch);
    }
}

void Interpreter::visitWhileStmt(const std::shared_ptr<WhileStmt>& stmt, int line, const std::string& filename) {
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

void Interpreter::visitRepeatStmt(const std::shared_ptr<RepeatStmt>& stmt, int line, const std::string& filename) {
    Value startVal = evaluate(stmt->start);
    Value endVal = evaluate(stmt->end);
    
    if (!startVal.isNumber() || !endVal.isNumber()) {
        runtimeError("Repeat bounds must be numbers", line, filename);
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

void Interpreter::visitGetStmt(const std::shared_ptr<GetStmt>& stmt, int line, const std::string& filename) {
    Value iterable = evaluate(stmt->iterable);
    
    if (!iterable.isArray() && !iterable.isString() && !iterable.isDictionary()) {
        runtimeError("Can only iterate over arrays, strings, and dictionaries", line, filename);
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

void Interpreter::visitTaskStmt(const std::shared_ptr<TaskStmt>& stmt, int line, const std::string& filename) {
    Value function = Value::makeFunction(stmt->name, stmt->params, stmt->defaultValues, stmt->body, currentEnv);
    currentEnv->define(stmt->name, function);
}

void Interpreter::visitGiveStmt(const std::shared_ptr<GiveStmt>& stmt, int line, const std::string& filename) {
    Value value;
    if (stmt->value) {
        value = evaluate(stmt->value);
    }
    throw ReturnException(value);
}

void Interpreter::visitEscapeStmt(const std::shared_ptr<EscapeStmt>&, int line, const std::string& filename) {
    throw BreakException();
}

void Interpreter::visitSkipStmt(const std::shared_ptr<SkipStmt>&, int line, const std::string& filename) {
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

Value Interpreter::callFunction(const Value& callee, const std::vector<Value>& args, int line, const std::string& filename) {
    if (callee.isNativeFunction()) {
        auto nativeFn = callee.asNativeFunction();
        if (nativeFn->arity != -1 && static_cast<int>(args.size()) != nativeFn->arity) {
            runtimeError("Expected " + std::to_string(nativeFn->arity) + 
                         " arguments but got " + std::to_string(args.size()), line, filename);
        }
        
        // Push to stack for native calls too (can be useful for debugging)
        callStack.push_back({nativeFn->name, filename, line});
        try {
            Value result = nativeFn->function(*this, args);
            callStack.pop_back();
            return result;
        } catch (...) {
            callStack.pop_back();
            throw;
        }
    }
    
    if (callee.isFunction()) {
        auto func = callee.asFunction();
        std::vector<Value> finalArgs = args;
        
        // Fill in missing arguments with default values if available
        for (size_t i = args.size(); i < func->params.size(); i++) {
            if (i < func->defaultValues.size() && func->defaultValues[i]) {
                finalArgs.push_back(evaluate(func->defaultValues[i]));
            } else {
                runtimeError("Expected " + std::to_string(func->params.size()) + 
                             " arguments but got " + std::to_string(args.size()), line, filename);
            }
        }
        
        if (finalArgs.size() > func->params.size()) {
            runtimeError("Expected " + std::to_string(func->params.size()) + 
                         " arguments but got " + std::to_string(finalArgs.size()), line, filename);
        }
        
        auto funcEnv = std::make_shared<Environment>(func->closure);
        for (size_t i = 0; i < func->params.size(); i++) {
            funcEnv->define(func->params[i], finalArgs[i]);
        }
        
        // Push frame
        callStack.push_back({func->name, filename, line});
        
        try {
            executeBlock(func->body, funcEnv);
            callStack.pop_back();
        } catch (const ReturnException& e) {
            callStack.pop_back();
            return e.value;
        } catch (...) {
            callStack.pop_back();
            throw;
        }
        
        return Value();
    }
    
    if (callee.isSuper()) {
        auto superObj = callee.asSuper();
        auto klass = superObj->parentKlass;
        
        if (args.size() != klass->initParams.size()) {
            runtimeError("Expected " + std::to_string(klass->initParams.size()) + 
                         " arguments for super init but got " + std::to_string(args.size()), line, filename);
        }
        
        if (!klass->initBody.empty()) {
            auto methodEnv = globalEnv->createChild(); 
            methodEnv->define("self", Value(superObj->instance));
            
            for (size_t i = 0; i < args.size(); i++) {
                methodEnv->define(klass->initParams[i], args[i]);
            }
            
            if (klass->parent) {
                methodEnv->define("super", Value::makeSuper(superObj->instance, klass->parent));
            }
            
            std::shared_ptr<Environment> previousEnv = currentEnv;
            currentEnv = methodEnv;
            
            // Push frame for super init
            callStack.push_back({klass->name + ".init", filename, line});
            
            try {
                executeBlock(klass->initBody, methodEnv);
                callStack.pop_back();
            } catch (const ReturnException&) {
                callStack.pop_back();
            } catch (...) {
                callStack.pop_back();
                currentEnv = previousEnv;
                throw;
            }
            currentEnv = previousEnv;
        }
        return Value();
    }
    
    if (callee.isClass()) {
        auto klass = callee.asClass();
        auto instance = std::make_shared<EZInstance>(klass);
        Value instanceVal(instance);
        
        // Check argument count match for init
        if (args.size() != klass->initParams.size()) {
            runtimeError("Expected " + std::to_string(klass->initParams.size()) + 
                         " arguments for init but got " + std::to_string(args.size()), line, filename);
        }
        
        // Run init method if present
        if (!klass->initBody.empty()) {
            auto methodEnv = globalEnv->createChild(); 
            methodEnv->define("self", instanceVal);
            
            // Define params
            for (size_t i = 0; i < args.size(); i++) {
                methodEnv->define(klass->initParams[i], args[i]);
            }
            
            if (klass->parent) {
                methodEnv->define("super", Value::makeSuper(instance, klass->parent));
            }
            
            // Execute init body
            std::shared_ptr<Environment> previousEnv = currentEnv;
            currentEnv = methodEnv;
            
            // Push frame for init
            callStack.push_back({klass->name + ".init", filename, line});
            
            try {
                executeBlock(klass->initBody, methodEnv);
                callStack.pop_back();
            } catch (const ReturnException&) {
                // init ignored return
                callStack.pop_back();
            } catch (...) {
                callStack.pop_back();
                currentEnv = previousEnv;
                throw;
            }
            
            currentEnv = previousEnv;
        }
        
        return instanceVal;
    }
    
    runtimeError("Can only call functions or models", line, filename);
    return Value(); // Unreachable
}

void Interpreter::checkNumberOperand(TokenType op, const Value& operand, int line, const std::string& filename) {
    if (!operand.isNumber()) {
        throw RuntimeError("Operand must be a number", line);
    }
}

void Interpreter::checkNumberOperands(TokenType op, const Value& left, const Value& right, int line, const std::string& filename) {
    if (!left.isNumber() || !right.isNumber()) {
        throw RuntimeError("Operands must be numbers", line);
    }
}

std::string Interpreter::stringify(const Value& val, int line, const std::string& filename) {
    if (val.isInstance()) {
        auto instance = val.asInstance();
        auto klass = instance->klass;
        while (klass) {
            auto it = klass->methods.find("toString");
            if (it != klass->methods.end()) {
                Value method = it->second;
                if (method.isFunction()) {
                    auto func = method.asFunction();
                    // Must have 0 parameters
                    if (func->params.empty()) {
                        auto boundEnv = func->closure->createChild();
                        boundEnv->define("self", val);
                        Value boundMethod = Value::makeFunction(func->name, func->params, func->defaultValues, func->body, boundEnv);
                        try {
                            Value result = callFunction(boundMethod, {}, line, filename);
                            return result.toString();
                        } catch (const RuntimeError& e) {
                            // Ignore errors and fall back
                        }
                    }
                }
            }
            klass = klass->parent;
        }
    }
    return val.toString();
}

// ============ OOP Visitors ============

Value Interpreter::visitSelf(const std::shared_ptr<SelfExpr>& expr, int line, const std::string& filename) {
    return currentEnv->get("self", line);
}

Value Interpreter::visitNew(const std::shared_ptr<NewExpr>& expr, int line, const std::string& filename) {
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

Value Interpreter::visitPropertyAccess(const std::shared_ptr<PropertyAccessExpr>& expr, int line, const std::string& filename) {
    Value object = evaluate(expr->object);
    
    if (object.isClass()) {
        auto klass = object.asClass();
        auto it = klass->staticMembers.find(expr->property);
        if (it != klass->staticMembers.end()) {
            Value member = it->second;
            if (member.isFunction()) {
                auto func = member.asFunction();
                auto boundEnv = func->closure->createChild();
                boundEnv->define("self", object);
                if (klass->parent) {
                    boundEnv->define("super", Value(klass->parent));
                }
                return Value::makeFunction(func->name, func->params, func->defaultValues, func->body, boundEnv);
            }
            return member;
        }
        throw RuntimeError("Undefined static property '" + expr->property + "'", line);
    }
    
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
                Value method = it->second;
                if (!method.isFunction()) return method;
                
                auto func = method.asFunction();
                auto boundEnv = func->closure->createChild();
                boundEnv->define("self", object);
                if (klass->parent) {
                    boundEnv->define("super", Value::makeSuper(instance, klass->parent));
                }
                return Value::makeFunction(func->name, func->params, func->defaultValues, func->body, boundEnv);
            }
            klass = klass->parent;
        }
        
        throw RuntimeError("Undefined property '" + expr->property + "'", line);
    } else if (object.isSuper()) {
        auto superObj = object.asSuper();
        auto klass = superObj->parentKlass;
        while (klass) {
            auto it = klass->methods.find(expr->property);
            // Visibility is technically all public from subclasses in EZ currently, 
            // since we removed strict private checks or they only crash if unshown
            if (it != klass->methods.end()) {
                Value method = it->second;
                if (!method.isFunction()) return method;
                
                auto func = method.asFunction();
                auto boundEnv = func->closure->createChild();
                boundEnv->define("self", Value(superObj->instance));
                if (klass->parent) {
                    boundEnv->define("super", Value::makeSuper(superObj->instance, klass->parent));
                }
                return Value::makeFunction(func->name, func->params, func->defaultValues, func->body, boundEnv);
            }
            klass = klass->parent;
        }
        throw RuntimeError("Undefined property '" + expr->property + "' on super", line);
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

void Interpreter::visitModelStmt(const std::shared_ptr<ModelStmt>& stmt, int line, const std::string& filename) {
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
                member.name, member.params, std::vector<ExprPtr>{}, member.body, globalEnv
            );
            
            if (member.isStatic) {
                klass->staticMembers[member.name] = method;
            } else {
                klass->methods[member.name] = method;
            }
        } else {
            // Properties are dynamic, but we can store visibility
            // For static properties, initialize them exactly once per class definition
            if (member.isStatic) {
                Value initVal;
                if (member.initializer) {
                    initVal = evaluate(member.initializer);
                }
                klass->staticMembers[member.name] = initVal;
            }
        }
    }
    
    // Validate interfaces
    for (const auto& ifaceName : stmt->interfaces) {
        auto it = definedInterfaces.find(ifaceName);
        if (it == definedInterfaces.end()) {
            runtimeError("Undefined interface '" + ifaceName + "'", line, filename);
        }
        for (const auto& requiredMethod : it->second) {
            if (klass->methods.find(requiredMethod) == klass->methods.end()) {
                runtimeError("Model '" + stmt->name + "' does not implement required method '" + requiredMethod + "' from interface '" + ifaceName + "'", line, filename);
            }
        }
    }
    
    globalEnv->define(stmt->name, Value(klass));
}

void Interpreter::visitInterfaceStmt(const std::shared_ptr<InterfaceStmt>& stmt, int line, const std::string& filename) {
    definedInterfaces[stmt->name] = stmt->methods;
}

Value Interpreter::visitSet(const std::shared_ptr<SetExpr>& expr, int line, const std::string& filename) {
    Value object = evaluate(expr->object);
    
    if (!object.isInstance() && !object.isDictionary() && !object.isClass()) {
        throw RuntimeError("Only instances, dictionaries, or classes have fields", line);
    }
    
    Value value = evaluate(expr->value);
    
    if (object.isClass()) {
        auto klass = object.asClass();
        klass->staticMembers[expr->name] = value;
        return value;
    }
    
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



Value Interpreter::visitDictionary(const std::shared_ptr<DictionaryExpr>& expr, int line, const std::string& filename) {
    auto dict = Value::makeDictionary();
    auto& map = dict.asDictionary().map;
    
    for (const auto& pair : expr->pairs) {
        Value key = evaluate(pair.first);
        Value val = evaluate(pair.second);
        map[key.toString()] = val;
    }
    return dict;
}

void Interpreter::visitStructStmt(const std::shared_ptr<StructStmt>& stmt, int line, const std::string& filename) {
    // Treat struct as a class with auto-generated init method
    auto klass = std::make_shared<EZClass>(stmt->name);
    
    // Init method params are the fields
    klass->initParams = stmt->fields;
    
    // Synthesize body for init: self.field = field
    for (const auto& field : stmt->fields) {
        // self
        auto selfExpr = makeSelfExpr(line, filename);
        // value (param)
        auto valExpr = makeIdentifierExpr(line, filename, field);
        // self.field = value
        auto setExpr = makeSetExpr(line, filename, selfExpr, field, valExpr);
        // Stmt
        klass->initBody.push_back(makeExprStmt(line, filename, setExpr));
    }
    
    defineGlobal(stmt->name, Value(klass));
}

void Interpreter::visitUseStmt(const std::shared_ptr<UseStmt>& stmt, int line, const std::string& filename) {
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
                         runtimeError("Could not find module '" + stmt->path + "'", line, filename);
                     }
                 }
             }
        }
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    
    Lexer lexer(source, path);
    auto tokens = lexer.tokenize();
    if (lexer.hasError()) {
        runtimeError("Lexer error in module '" + path + "'", line, filename);
    }
    
    Parser parser(tokens);
    auto statements = parser.parse();
    if (parser.hasError()) {
         runtimeError("Parser error in module '" + path + "'", line, filename);
    }
    
    for (const auto& s : statements) {
        execute(s);
    }
}

void Interpreter::visitTryStmt(const std::shared_ptr<TryStmt>& stmt, int line, const std::string& filename) {
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

void Interpreter::visitThrowStmt(const std::shared_ptr<ThrowStmt>& stmt, int line, const std::string& filename) {
    Value value = evaluate(stmt->expression);
    runtimeError(stringify(value, line, filename), line, filename);
}
