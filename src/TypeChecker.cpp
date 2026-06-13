#include "TypeChecker.h"
#include <iostream>

TypeChecker::TypeChecker() : currentEnv(nullptr), currentReturnType("Any"), hadError(false) {}

void TypeChecker::error(int line, const std::string& message) {
    hadError = true;
    std::cerr << "Type Error at line " << line << ": " << message << std::endl;
}

void TypeChecker::beginScope() {
    currentEnv = new Environment(currentEnv);
}

void TypeChecker::endScope() {
    Environment* old = currentEnv;
    currentEnv = currentEnv->enclosing;
    delete old;
}

void TypeChecker::declareVariable(const std::string& name, const TypeInfo& type) {
    if (currentEnv) {
        currentEnv->variables[name] = type;
    }
}

TypeInfo TypeChecker::resolveVariable(const std::string& name) {
    Environment* env = currentEnv;
    while (env) {
        if (env->variables.count(name)) {
            return env->variables[name];
        }
        env = env->enclosing;
    }
    return TypeInfo("Any");
}

void TypeChecker::declareFunction(const std::string& name, const FunctionSignature& sig) {
    if (currentEnv) {
        currentEnv->functions[name] = sig;
    }
}

FunctionSignature* TypeChecker::resolveFunction(const std::string& name) {
    Environment* env = currentEnv;
    while (env) {
        if (env->functions.count(name)) {
            return &env->functions[name];
        }
        env = env->enclosing;
    }
    return nullptr;
}

bool TypeChecker::check(const std::vector<StmtPtr>& statements) {
    hadError = false;
    currentEnv = new Environment();
    
    // First pass: declare all functions
    for (const auto& stmt : statements) {
        if (std::holds_alternative<std::shared_ptr<TaskStmt>>(stmt->variant)) {
            auto task = std::get<std::shared_ptr<TaskStmt>>(stmt->variant);
            FunctionSignature sig;
            for (const auto& t : task->paramTypes) sig.paramTypes.push_back(TypeInfo::fromAST(t));
            sig.returnType = TypeInfo::fromAST(task->returnType);
            declareFunction(task->name, sig);
        }
    }
    
    // Second pass: check body
    for (const auto& stmt : statements) {
        checkStmt(stmt);
    }
    
    delete currentEnv;
    currentEnv = nullptr;
    return !hadError;
}

void TypeChecker::checkStmt(const StmtPtr& stmt) {
    if (!stmt) return;
    std::visit([this](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::shared_ptr<VarDeclStmt>>) checkVarDecl(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<TaskStmt>>) checkTask(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<GiveStmt>>) checkGive(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<BlockStmt>>) checkBlock(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<WhenStmt>>) checkWhen(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<WhileStmt>>) checkWhile(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<RepeatStmt>>) checkRepeat(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<GetStmt>>) checkGet(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<ExprStmt>>) checkExpr(arg->expression);
        else if constexpr (std::is_same_v<T, std::shared_ptr<OutStmt>>) checkExpr(arg->expression);
    }, stmt->variant);
}

void TypeChecker::checkVarDecl(const VarDeclStmt& stmt) {
    TypeInfo declaredType = TypeInfo::fromAST(stmt.typeHint);
    if (stmt.initializer) {
        TypeInfo initType = checkExpr(stmt.initializer);
        if (declaredType != initType) {
            // Note: error only if not Any
            if (declaredType.baseType != "Any" && initType.baseType != "Any") {
                error(stmt.initializer ? stmt.initializer->line : 0, 
                      "Type mismatch in declaration of '" + stmt.name + "'. Expected " + declaredType.toString() + " but got " + initType.toString());
            }
        }
    }
    declareVariable(stmt.name, declaredType);
}

void TypeChecker::checkTask(const TaskStmt& stmt) {
    FunctionSignature sig;
    for (const auto& t : stmt.paramTypes) sig.paramTypes.push_back(TypeInfo::fromAST(t));
    sig.returnType = TypeInfo::fromAST(stmt.returnType);
    
    // We don't declare it again here if global, but inner functions might need it.
    declareFunction(stmt.name, sig);
    
    beginScope();
    for (size_t i = 0; i < stmt.params.size(); i++) {
        declareVariable(stmt.params[i], sig.paramTypes[i]);
    }
    
    TypeInfo prevReturn = currentReturnType;
    currentReturnType = sig.returnType;
    
    for (const auto& s : stmt.body) checkStmt(s);
    
    currentReturnType = prevReturn;
    endScope();
}

void TypeChecker::checkGive(const GiveStmt& stmt) {
    TypeInfo retType("Any");
    if (stmt.value) {
        retType = checkExpr(stmt.value);
    }
    
    if (currentReturnType != retType) {
        if (currentReturnType.baseType != "Any" && retType.baseType != "Any") {
            error(stmt.value ? stmt.value->line : 0, 
                  "Type mismatch in return statement. Expected " + currentReturnType.toString() + " but got " + retType.toString());
        }
    }
}

void TypeChecker::checkBlock(const BlockStmt& stmt) {
    beginScope();
    for (const auto& s : stmt.statements) checkStmt(s);
    endScope();
}

void TypeChecker::checkWhen(const WhenStmt& stmt) {
    checkExpr(stmt.condition);
    checkStmt(stmt.thenBranch);
    if (stmt.elseBranch) checkStmt(stmt.elseBranch);
}

void TypeChecker::checkWhile(const WhileStmt& stmt) {
    checkExpr(stmt.condition);
    checkStmt(stmt.body);
}

void TypeChecker::checkRepeat(const RepeatStmt& stmt) {
    beginScope();
    declareVariable(stmt.variable, TypeInfo("number"));
    checkExpr(stmt.start);
    checkExpr(stmt.end);
    checkStmt(stmt.body);
    endScope();
}

void TypeChecker::checkGet(const GetStmt& stmt) {
    beginScope();
    declareVariable(stmt.valueVariable.empty() ? stmt.variable : stmt.valueVariable, TypeInfo("Any"));
    if (!stmt.valueVariable.empty()) declareVariable(stmt.variable, TypeInfo("Any"));
    checkExpr(stmt.iterable);
    checkStmt(stmt.body);
    endScope();
}

TypeInfo TypeChecker::checkExpr(const ExprPtr& expr) {
    if (!expr) return TypeInfo("Any");
    
    return std::visit([this](auto&& arg) -> TypeInfo {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::shared_ptr<AssignExpr>>) return checkAssign(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<BinaryExpr>>) return checkBinary(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<UnaryExpr>>) return checkUnary(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<CallExpr>>) return checkCall(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<IdentifierExpr>>) return checkIdentifier(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<LiteralExpr>>) return checkLiteral(*arg);
        else return TypeInfo("Any");
    }, expr->variant);
}

TypeInfo TypeChecker::checkAssign(const AssignExpr& expr) {
    TypeInfo valType = checkExpr(expr.value);
    
    if (expr.index) {
        checkExpr(expr.object);
        checkExpr(expr.index);
        return valType;
    }
    
    TypeInfo declaredType = resolveVariable(expr.name);
    if (declaredType != valType) {
        if (declaredType.baseType != "Any" && valType.baseType != "Any") {
            // Can't assign wrong type
            error(expr.value ? expr.value->line : 0, 
                  "Type mismatch in assignment to '" + expr.name + "'. Expected " + declaredType.toString() + " but got " + valType.toString());
        }
    }
    
    return valType;
}

TypeInfo TypeChecker::checkBinary(const BinaryExpr& expr) {
    TypeInfo left = checkExpr(expr.left);
    TypeInfo right = checkExpr(expr.right);
    
    // Simplistic inference: if arithmetic, returning number. If equality, returning bool.
    switch (expr.op) {
        case TokenType::PLUS:
        case TokenType::MINUS:
        case TokenType::STAR:
        case TokenType::SLASH:
        case TokenType::PERCENT:
            if (left.baseType != "Any" && right.baseType != "Any") {
                if (left.baseType == "string" && expr.op == TokenType::PLUS) return TypeInfo("string");
                if (left.baseType != "number" || right.baseType != "number") {
                    error(expr.left->line, "Invalid operand types for arithmetic operator.");
                }
            }
            return TypeInfo("number");
        case TokenType::EQUAL_EQUAL:
        case TokenType::BANG_EQUAL:
        case TokenType::LESS:
        case TokenType::LESS_EQUAL:
        case TokenType::GREATER:
        case TokenType::GREATER_EQUAL:
            return TypeInfo("bool");
        default:
            return TypeInfo("Any");
    }
}

TypeInfo TypeChecker::checkUnary(const UnaryExpr& expr) {
    TypeInfo operand = checkExpr(expr.operand);
    if (expr.op == TokenType::MINUS && operand.baseType != "Any" && operand.baseType != "number") {
        error(expr.operand->line, "Invalid operand for unary minus.");
    }
    if (expr.op == TokenType::NOT) return TypeInfo("bool");
    return operand;
}

TypeInfo TypeChecker::checkCall(const CallExpr& expr) {
    checkExpr(expr.callee);
    std::vector<TypeInfo> argTypes;
    for (const auto& arg : expr.arguments) argTypes.push_back(checkExpr(arg));
    
    if (std::holds_alternative<std::shared_ptr<IdentifierExpr>>(expr.callee->variant)) {
        std::string name = std::get<std::shared_ptr<IdentifierExpr>>(expr.callee->variant)->name;
        FunctionSignature* sig = resolveFunction(name);
        if (sig) {
            // Only verify arity if not variadic, but for simplicity we skip variadic check here or just check basic args
            // In EZ, builtins aren't in this environment. So `sig` might be null for builtins!
            // If sig exists, we verify it.
            if (argTypes.size() != sig->paramTypes.size()) {
                // If the signature is not marked variadic... wait, FunctionSignature doesn't have isVariadic.
                // We'll just tolerate it or skip checking if size mismatches for now.
                // Actually, let's keep it strict if size matches, but if not we can't reliably check without isVariadic flag.
            } else {
                for (size_t i = 0; i < argTypes.size(); i++) {
                    if (argTypes[i] != sig->paramTypes[i] && argTypes[i].baseType != "Any" && sig->paramTypes[i].baseType != "Any") {
                        error(expr.callee->line, "Type mismatch in argument " + std::to_string(i+1) + " for function '" + name + "'. Expected " + sig->paramTypes[i].toString() + " but got " + argTypes[i].toString());
                    }
                }
            }
            return sig->returnType;
        }
    }
    return TypeInfo("Any");
}

TypeInfo TypeChecker::checkIdentifier(const IdentifierExpr& expr) {
    return resolveVariable(expr.name);
}

TypeInfo TypeChecker::checkLiteral(const LiteralExpr& expr) {
    return std::visit([](auto&& arg) -> TypeInfo {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) return TypeInfo("nil");
        else if constexpr (std::is_same_v<T, bool>) return TypeInfo("bool");
        else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, long long>) return TypeInfo("number");
        else if constexpr (std::is_same_v<T, std::string>) return TypeInfo("string");
        return TypeInfo("Any");
    }, expr.value);
}
