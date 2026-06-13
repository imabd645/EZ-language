#include "TypeChecker.h"
#include <iostream>
#include <fstream>

TypeChecker::TypeChecker() : currentEnv(nullptr), currentReturnType("Any"), hadError(false) {}

void TypeChecker::error(const ExprPtr& expr, const std::string& message, const std::string& hint) {
    if (expr) error(expr->line, expr->column, expr->length, expr->filename, message, hint);
    else error(0, 0, 0, "", message, hint);
}

void TypeChecker::error(const StmtPtr& stmt, const std::string& message, const std::string& hint) {
    if (stmt) error(stmt->line, stmt->column, stmt->length, stmt->filename, message, hint);
    else error(0, 0, 0, "", message, hint);
}

void TypeChecker::error(int line, int column, int length, const std::string& filename, const std::string& message, const std::string& hint) {
    hadError = true;
    std::cerr << "Type Error at line " << line << ", column " << column << ":\n\n";

    if (!filename.empty()) {
        std::ifstream file(filename);
        if (file.is_open()) {
            std::string sourceLine;
            for (int i = 1; i <= line; ++i) {
                if (!std::getline(file, sourceLine)) break;
            }
            if (!sourceLine.empty()) {
                std::cerr << "  " << sourceLine << "\n";
                if (column > 0) {
                    std::cerr << "  " << std::string(column - 1, ' ');
                    int caretLen = length > 0 ? length : 1;
                    std::cerr << std::string(caretLen, '^') << "\n";
                }
            }
        }
    }
    
    std::cerr << "  " << message << "\n";
    if (!hint.empty()) {
        std::cerr << "\n  Hint: " << hint << "\n";
    }
    std::cerr << "\n";
}

void TypeChecker::error(int line, const std::string& message) {
    error(line, 0, 0, "", message, "");
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
            for (const auto& p : task->params) sig.paramNames.push_back(p);
            for (const auto& t : task->paramTypes) sig.paramTypes.push_back(TypeInfo::fromAST(t));
            sig.returnType = TypeInfo::fromAST(task->returnType);
            declareFunction(task->name, sig);
        } else if (std::holds_alternative<std::shared_ptr<ModelStmt>>(stmt->variant)) {
            auto model = std::get<std::shared_ptr<ModelStmt>>(stmt->variant);
            FunctionSignature sig;
            for (const auto& p : model->initParams) sig.paramNames.push_back(p);
            for (const auto& t : model->initParamTypes) sig.paramTypes.push_back(TypeInfo::fromAST(t));
            sig.returnType = TypeInfo(model->name);
            declareFunction(model->name, sig);
        } else if (std::holds_alternative<std::shared_ptr<StructStmt>>(stmt->variant)) {
            auto structStmt = std::get<std::shared_ptr<StructStmt>>(stmt->variant);
            FunctionSignature sig;
            for (size_t i = 0; i < structStmt->fields.size(); ++i) {
                sig.paramNames.push_back(structStmt->fields[i]);
                sig.paramTypes.push_back(TypeInfo("Any")); // Assuming structs don't have explicit param types for now
            }
            sig.returnType = TypeInfo(structStmt->name);
            declareFunction(structStmt->name, sig);
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
        else if constexpr (std::is_same_v<T, std::shared_ptr<ModelStmt>>) checkModel(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<StructStmt>>) checkStruct(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<InterfaceStmt>>) checkInterface(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<TryStmt>>) checkTry(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<ThrowStmt>>) checkThrow(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<MatchStmt>>) checkMatch(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<StaticStmt>>) checkStatic(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<EscapeStmt>>) {} // No checking needed
        else if constexpr (std::is_same_v<T, std::shared_ptr<SkipStmt>>) {} // No checking needed
        else if constexpr (std::is_same_v<T, std::shared_ptr<UseStmt>>) {} // No checking needed
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

void TypeChecker::checkModel(const ModelStmt& stmt) {
    std::string previousModel = currentModel;
    currentModel = stmt.name;
    beginScope();
    
    // Check init method
    beginScope();
    for (size_t i = 0; i < stmt.initParams.size(); ++i) {
        declareVariable(stmt.initParams[i], TypeInfo::fromAST(stmt.initParamTypes[i]));
    }
    for (const auto& s : stmt.initBody) checkStmt(s);
    endScope();
    
    // Check members
    for (const auto& member : stmt.members) {
        if (member.isMethod) {
            beginScope();
            TypeInfo oldRet = currentReturnType;
            currentReturnType = TypeInfo::fromAST(member.typeHint);
            
            for (size_t i = 0; i < member.params.size(); ++i) {
                declareVariable(member.params[i], TypeInfo::fromAST(member.paramTypes[i]));
            }
            for (const auto& s : member.body) checkStmt(s);
            
            currentReturnType = oldRet;
            endScope();
        } else {
            if (member.initializer) {
                TypeInfo initType = checkExpr(member.initializer);
                TypeInfo declaredType = TypeInfo::fromAST(member.typeHint);
                if (declaredType.baseType != "Any" && initType.baseType != "Any" && declaredType != initType) {
                    error(member.initializer ? member.initializer : nullptr, "Type mismatch in property '" + member.name + "' initialization.", "Expected " + declaredType.toString() + " but got " + initType.toString());
                }
            }
        }
    }
    
    endScope();
    currentModel = previousModel;
}

void TypeChecker::checkStruct(const StructStmt& stmt) {}

void TypeChecker::checkInterface(const InterfaceStmt& stmt) {}

void TypeChecker::checkTry(const TryStmt& stmt) {
    checkStmt(stmt.tryBlock);
    for (const auto& catchBlock : stmt.catchBlocks) {
        beginScope();
        declareVariable(catchBlock.varName, catchBlock.typeName.empty() ? TypeInfo("Any") : TypeInfo(catchBlock.typeName));
        checkStmt(catchBlock.body);
        endScope();
    }
}

void TypeChecker::checkThrow(const ThrowStmt& stmt) {
    checkExpr(stmt.expression);
}

void TypeChecker::checkMatch(const MatchStmt& stmt) {
    checkExpr(stmt.subject);
    for (const auto& arm : stmt.arms) {
        if (arm.pattern) checkExpr(arm.pattern);
        beginScope();
        checkStmt(arm.body);
        endScope();
    }
}

void TypeChecker::checkStatic(const StaticStmt& stmt) {
    TypeInfo initType = checkExpr(stmt.initializer);
    declareVariable(stmt.name, initType);
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
        else if constexpr (std::is_same_v<T, std::shared_ptr<LambdaExpr>>) return checkLambda(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<PropertyAccessExpr>>) return checkPropertyAccess(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<SetExpr>>) return checkSet(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<SelfExpr>>) return checkSelf(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<NewExpr>>) return checkNew(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<IndexExpr>>) return checkIndex(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<DictionaryExpr>>) return checkDictionary(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<SpreadExpr>>) return checkSpread(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<TernaryExpr>>) return checkTernary(*arg);
        else if constexpr (std::is_same_v<T, std::shared_ptr<AwaitExpr>>) return checkAwait(*arg);
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
    
    if (expr.op == TokenType::PLUS || expr.op == TokenType::MINUS || expr.op == TokenType::STAR || expr.op == TokenType::SLASH) {
        if (left.baseType != "Any" && right.baseType != "Any") {
            if (left.baseType != right.baseType || (left.baseType != "number" && left.baseType != "string")) {
                error(expr.left, "Invalid operand types for arithmetic operator.", "Got " + left.toString() + " and " + right.toString());
            }
        }
        return left.baseType == "Any" ? right : left;
    } else if (expr.op == TokenType::EQUAL_EQUAL || expr.op == TokenType::BANG_EQUAL || 
               expr.op == TokenType::LESS || expr.op == TokenType::LESS_EQUAL ||
               expr.op == TokenType::GREATER || expr.op == TokenType::GREATER_EQUAL) {
        return TypeInfo("bool");
    }
    
    return TypeInfo("Any");
}

TypeInfo TypeChecker::checkUnary(const UnaryExpr& expr) {
    TypeInfo operand = checkExpr(expr.operand);
    if (expr.op == TokenType::MINUS && operand.baseType != "Any" && operand.baseType != "number") {
        error(expr.operand, "Invalid operand for unary minus.", "Expected number but got " + operand.toString());
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
                        std::string hint = "";
                        if (sig->paramTypes[i].baseType == "number" && argTypes[i].baseType == "string") {
                            if (std::holds_alternative<std::shared_ptr<LiteralExpr>>(expr.arguments[i]->variant)) {
                                auto lit = std::get<std::shared_ptr<LiteralExpr>>(expr.arguments[i]->variant);
                                if (std::holds_alternative<std::string>(lit->value)) {
                                    std::string strVal = std::get<std::string>(lit->value);
                                    hint = "Did you mean " + strVal + " instead of \"" + strVal + "\"?";
                                }
                            }
                            if (hint.empty()) hint = "Did you mean to pass a number instead of a string?";
                        }
                        
                        std::string signatureStr = "Function signature: " + name + "(";
                        for (size_t j = 0; j < sig->paramTypes.size(); ++j) {
                            if (j < sig->paramNames.size()) signatureStr += sig->paramNames[j] + ":";
                            signatureStr += sig->paramTypes[j].toString();
                            if (j + 1 < sig->paramTypes.size()) signatureStr += ", ";
                        }
                        signatureStr += ")";
                        
                        std::string paramNameStr = (i < sig->paramNames.size()) ? (" '" + sig->paramNames[i] + "'") : "";
                        std::string msg = "Argument " + std::to_string(i+1) + paramNameStr + " expects " + sig->paramTypes[i].toString() + " but got " + argTypes[i].toString();
                        
                        error(expr.arguments[i], msg, signatureStr + "\n  Hint: " + hint);
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

TypeInfo TypeChecker::checkLambda(const LambdaExpr& expr) {
    beginScope();
    for (size_t i = 0; i < expr.params.size(); ++i) {
        declareVariable(expr.params[i], TypeInfo::fromAST(expr.paramTypes[i]));
    }
    
    TypeInfo oldRet = currentReturnType;
    currentReturnType = TypeInfo::fromAST(expr.returnType);
    
    if (expr.body) checkExpr(expr.body);
    else for (const auto& s : expr.stmtBody) checkStmt(s);
    
    currentReturnType = oldRet;
    endScope();
    
    return TypeInfo("Any");
}

TypeInfo TypeChecker::checkPropertyAccess(const PropertyAccessExpr& expr) {
    checkExpr(expr.object);
    return TypeInfo("Any");
}

TypeInfo TypeChecker::checkSet(const SetExpr& expr) {
    checkExpr(expr.object);
    return checkExpr(expr.value);
}

TypeInfo TypeChecker::checkSelf(const SelfExpr& expr) {
    if (currentModel.empty()) {
        // error: 'self' outside of model (ignoring for now to not break dynamic usage)
        return TypeInfo("Any");
    }
    return TypeInfo(currentModel);
}

TypeInfo TypeChecker::checkNew(const NewExpr& expr) {
    for (const auto& arg : expr.arguments) checkExpr(arg);
    return TypeInfo(expr.className);
}

TypeInfo TypeChecker::checkIndex(const IndexExpr& expr) {
    checkExpr(expr.object);
    checkExpr(expr.index);
    return TypeInfo("Any");
}

TypeInfo TypeChecker::checkDictionary(const DictionaryExpr& expr) {
    for (const auto& pair : expr.pairs) {
        checkExpr(pair.first);
        checkExpr(pair.second);
    }
    return TypeInfo("Any");
}

TypeInfo TypeChecker::checkSpread(const SpreadExpr& expr) {
    return checkExpr(expr.expression);
}

TypeInfo TypeChecker::checkTernary(const TernaryExpr& expr) {
    checkExpr(expr.condition);
    TypeInfo thenType = checkExpr(expr.thenBranch);
    TypeInfo elseType = checkExpr(expr.elseBranch);
    return thenType == elseType ? thenType : TypeInfo("Any");
}

TypeInfo TypeChecker::checkAwait(const AwaitExpr& expr) {
    return checkExpr(expr.expression);
}
