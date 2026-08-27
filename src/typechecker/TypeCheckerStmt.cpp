#include "typechecker/TypeChecker.h"
#include <iostream>

bool TypeChecker::isTerminal(const StmtPtr& stmt) const {
    if (!stmt) return false;
    return std::visit([this](auto&& arg) -> bool {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, GiveStmt*>) return true;
        if constexpr (std::is_same_v<T, ThrowStmt*>) return true;
        if constexpr (std::is_same_v<T, EscapeStmt*>) return true;
        if constexpr (std::is_same_v<T, SkipStmt*>) return true;
        if constexpr (std::is_same_v<T, BlockStmt*>) {
            for (const auto& s : arg->statements) {
                if (isTerminal(s)) return true;
            }
            return false;
        }
        if constexpr (std::is_same_v<T, WhenStmt*>) {
            if (arg->elseBranch) {
                return isTerminal(arg->thenBranch) && isTerminal(arg->elseBranch);
            }
            return false;
        }
        if constexpr (std::is_same_v<T, TryStmt*>) {
            bool tryTerm = isTerminal(arg->tryBlock);
            if (!tryTerm) return false;
            for (const auto& cb : arg->catchBlocks) {
                if (!isTerminal(cb.body)) return false;
            }
            return true;
        }
        if constexpr (std::is_same_v<T, MatchStmt*>) {
            bool allCasesTerminal = true;
            bool hasDefault = false;
            for (const auto& arm : arg->arms) {
                if (!arm.pattern) {
                    hasDefault = true;
                }
                if (!isTerminal(arm.body)) {
                    allCasesTerminal = false;
                    break;
                }
            }
            return hasDefault && allCasesTerminal;
        }
        return false;
    }, stmt->variant);
}

void TypeChecker::checkStmt(const StmtPtr& stmt) {
    if (!stmt) return;
    std::visit([this](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, VarDeclStmt*>) checkVarDecl(*arg);
        else if constexpr (std::is_same_v<T, TaskStmt*>) checkTask(*arg);
        else if constexpr (std::is_same_v<T, GiveStmt*>) checkGive(*arg);
        else if constexpr (std::is_same_v<T, BlockStmt*>) checkBlock(*arg);
        else if constexpr (std::is_same_v<T, WhenStmt*>) checkWhen(*arg);
        else if constexpr (std::is_same_v<T, WhileStmt*>) checkWhile(*arg);
        else if constexpr (std::is_same_v<T, RepeatStmt*>) checkRepeat(*arg);
        else if constexpr (std::is_same_v<T, GetStmt*>) checkGet(*arg);
        else if constexpr (std::is_same_v<T, ExpressionStmt*>) checkExpr(arg->expr);
        else if constexpr (std::is_same_v<T, OutStmt*>) checkExpr(arg->expr);
        else if constexpr (std::is_same_v<T, ModelStmt*>) checkModel(*arg);
        else if constexpr (std::is_same_v<T, StructStmt*>) checkStruct(*arg);
        else if constexpr (std::is_same_v<T, InterfaceStmt*>) checkInterface(*arg);
        else if constexpr (std::is_same_v<T, TryStmt*>) checkTry(*arg);
        else if constexpr (std::is_same_v<T, ThrowStmt*>) checkThrow(*arg);
        else if constexpr (std::is_same_v<T, MatchStmt*>) checkMatch(*arg);
        else if constexpr (std::is_same_v<T, StaticStmt*>) checkStatic(*arg);
        else if constexpr (std::is_same_v<T, EscapeStmt*>) {
            if (loopDepth == 0) {
                error(0, "break or continue statement outside of a loop"); // EscapeStmt does not store line currently
            }
        }
        else if constexpr (std::is_same_v<T, SkipStmt*>) {} // No checking needed
        else if constexpr (std::is_same_v<T, UseStmt*>) {
            hasImports = true;
            if (!arg->alias.empty()) {
                declareVariable(arg->alias, TypeInfo("Any"));
            }
        }
        else if constexpr (std::is_same_v<T, ExportStmt*>) {
            checkStmt(arg->inner); // Delegate to the inner declaration
        }
    }, stmt->variant);
}

void TypeChecker::checkVarDecl(const VarDeclStmt& stmt) {
    TypeInfo declaredType = TypeInfo::fromAST(stmt.typeHint);
    if (stmt.initializer) {
        TypeInfo initType = checkExpr(stmt.initializer);
        if (initType.baseType != "Any" && declaredType != initType) {
            error(stmt.initializer,
                  "Type mismatch in variable declaration.", "Expected " + declaredType.toString() + " but got " + initType.toString());
        }
    }
    declareVariable(stmt.name, declaredType);
}

void TypeChecker::checkTask(const TaskStmt& stmt) {
    FunctionSignature sig;
    for (const auto& p : stmt.params) sig.paramNames.push_back(p);
    for (const auto& t : stmt.paramTypes) sig.paramTypes.push_back(TypeInfo::fromAST(t));
    sig.returnType = TypeInfo::fromAST(stmt.returnType);
    sig.isVariadic = stmt.isVariadic;
    size_t minArgs = 0;
    for (const auto& dv : stmt.defaultValues) {
        if (!dv) minArgs++;
    }
    sig.minArgs = minArgs;
    
    // We don't declare it again here if global, but inner functions might need it.
    declareFunction(stmt.name, sig);
    
    beginScope();
    for (size_t i = 0; i < stmt.params.size(); i++) {
        declareVariable(stmt.params[i], sig.paramTypes[i]);
    }
    
    TypeInfo prevReturn = currentReturnType;
    currentReturnType = sig.returnType;
    
    bool isUnreachable = false;
    for (const auto& s : stmt.body) {
        if (isUnreachable) {
            warn(s, "Unreachable code detected.", "This statement follows a return, throw, break, or continue and will never be executed.");
            break;
        }
        checkStmt(s);
        if (isTerminal(s)) {
            isUnreachable = true;
        }
    }
    
    currentReturnType = prevReturn;
    endScope();
}

void TypeChecker::checkGive(const GiveStmt& stmt) {
    TypeInfo retType("Any");
    if (stmt.value) {
        retType = checkExpr(stmt.value);
    }
    
    TypeInfo expectedType = currentReturnType;
    if ((expectedType.baseType == "Task" || expectedType.baseType == "Future") && expectedType.typeArgs.size() == 1) {
        expectedType = expectedType.typeArgs[0];
    }
    
    if (expectedType != retType) {
        if (expectedType.baseType != "Any" && retType.baseType != "Any") {
            warn(stmt.value, 
                  "Type mismatch in return statement. Expected " + expectedType.toString() + " but got " + retType.toString());
        }
    }
}

void TypeChecker::checkBlock(const BlockStmt& stmt) {
    beginScope();
    bool isUnreachable = false;
    for (const auto& s : stmt.statements) {
        if (isUnreachable) {
            warn(s, "Unreachable code detected.", "This statement follows a return, throw, break, or continue and will never be executed.");
            break;
        }
        checkStmt(s);
        if (isTerminal(s)) {
            isUnreachable = true;
        }
    }
    endScope();
}

void TypeChecker::checkWhen(const WhenStmt& stmt) {
    checkExpr(stmt.condition);
    checkStmt(stmt.thenBranch);
    if (stmt.elseBranch) checkStmt(stmt.elseBranch);
}

void TypeChecker::checkWhile(const WhileStmt& stmt) {
    checkExpr(stmt.condition);
    loopDepth++;
    checkStmt(stmt.body);
    loopDepth--;
}

void TypeChecker::checkRepeat(const RepeatStmt& stmt) {
    beginScope();
    declareVariable(stmt.variable, TypeInfo("number"));
    checkExpr(stmt.start);
    checkExpr(stmt.end);
    loopDepth++;
    checkStmt(stmt.body);
    loopDepth--;
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
    declaredModels.insert(stmt.name);
    if (!stmt.typeParams.empty()) {
        genericParameters[stmt.name] = stmt.typeParams;
    }
    if (!stmt.parentName.empty()) {
        modelHierarchy[stmt.name] = stmt.parentName;
    }
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

void TypeChecker::checkStruct(const StructStmt& stmt) {
    if (!stmt.typeParams.empty()) {
        genericParameters[stmt.name] = stmt.typeParams;
    }
    beginScope();
    for (size_t i = 0; i < stmt.fields.size(); ++i) {
        TypeInfo declaredType = TypeInfo::fromAST(stmt.types[i]);
        if (stmt.defaults[i]) {
            TypeInfo defaultType = checkExpr(stmt.defaults[i]);
            if (declaredType.baseType != "Any" && defaultType.baseType != "Any" && declaredType != defaultType) {
                error(stmt.defaults[i], "Type mismatch in struct field default value.", "Expected " + declaredType.toString() + " but got " + defaultType.toString());
            }
        }
        declareVariable(stmt.fields[i], declaredType);
    }
    endScope();
}

void TypeChecker::checkInterface(const InterfaceStmt& stmt) {
    // Interface currently only defines signatures which are registered in first pass (if they were).
    // Let's just traverse it in case we add default implementations later.
}

void TypeChecker::checkTry(const TryStmt& stmt) {
    checkStmt(stmt.tryBlock);
    for (const auto& catchBlock : stmt.catchBlocks) {
        beginScope();
        declareVariable(catchBlock.varName, catchBlock.typeName.empty() ? TypeInfo("Any") : TypeInfo(catchBlock.typeName));
        checkStmt(catchBlock.body);
        endScope();
    }
    if (stmt.finallyBlock) {
        checkStmt(stmt.finallyBlock);
    }
}

void TypeChecker::checkThrow(const ThrowStmt& stmt) {
    checkExpr(stmt.expr);
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

