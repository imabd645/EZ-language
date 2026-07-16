#include <algorithm>
#include "TypeChecker.h"
#include <iostream>
TypeInfo TypeChecker::checkExpr(const ExprPtr& expr) {
    if (!expr) return TypeInfo("Any");
    
    ExprPtr prevExprContext = currentExprContext;
    currentExprContext = expr;
    
    TypeInfo result = std::visit([this](auto&& arg) -> TypeInfo {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, AssignExpr*>) return checkAssign(*arg);
        else if constexpr (std::is_same_v<T, DestructureAssignExpr*>) return checkDestructureAssign(*arg);
        else if constexpr (std::is_same_v<T, BinaryExpr*>) return checkBinary(*arg);
        else if constexpr (std::is_same_v<T, UnaryExpr*>) return checkUnary(*arg);
        else if constexpr (std::is_same_v<T, CallExpr*>) return checkCall(*arg);
        else if constexpr (std::is_same_v<T, IdentifierExpr*>) return checkIdentifier(*arg);
        else if constexpr (std::is_same_v<T, LiteralExpr*>) return checkLiteral(*arg);
        else if constexpr (std::is_same_v<T, LambdaExpr*>) return checkLambda(*arg);
        else if constexpr (std::is_same_v<T, PropertyAccessExpr*>) return checkPropertyAccess(*arg);
        else if constexpr (std::is_same_v<T, SetExpr*>) return checkSet(*arg);
        else if constexpr (std::is_same_v<T, SelfExpr*>) return checkSelf(*arg);
        else if constexpr (std::is_same_v<T, SuperExpr*>) return checkSuper(*arg);
        else if constexpr (std::is_same_v<T, NewExpr*>) return checkNew(*arg);
        else if constexpr (std::is_same_v<T, IndexExpr*>) return checkIndex(*arg);
        else if constexpr (std::is_same_v<T, ArrayExpr*>) return checkArray(*arg);
        else if constexpr (std::is_same_v<T, TupleExpr*>) return checkTuple(*arg);
        else if constexpr (std::is_same_v<T, DictionaryExpr*>) return checkDictionary(*arg);
        else if constexpr (std::is_same_v<T, SpreadExpr*>) return checkSpread(*arg);
        else if constexpr (std::is_same_v<T, TernaryExpr*>) return checkTernary(*arg);
        else if constexpr (std::is_same_v<T, AwaitExpr*>) return checkAwait(*arg);
        else return TypeInfo("Any");
    }, expr->variant);
    
    currentExprContext = prevExprContext;
    return result;
}

TypeInfo TypeChecker::checkAssign(const AssignExpr& expr) {
    TypeInfo valType = checkExpr(expr.value);
    
    if (expr.index) {
        TypeInfo objType = checkExpr(expr.object);
        TypeInfo indexType = checkExpr(expr.index);
        
        if (objType.baseType == "Array" && objType.typeArgs.size() == 1) {
            if (indexType.baseType != "Any" && indexType.baseType != "number") {
                error(expr.index, "Array index must be a number");
            }
            if (valType.baseType != "Any" && objType.typeArgs[0].baseType != "Any" && valType != objType.typeArgs[0]) {
                error(expr.value, "Type mismatch in array assignment. Expected " + objType.typeArgs[0].toString() + " but got " + valType.toString());
            }
        } else if (objType.baseType == "Dict" && objType.typeArgs.size() == 2) {
            // Compare with operator!=, which is Any-aware on BOTH sides (and
            // compares nested type args). The old code compared raw baseType
            // strings, so it bypassed that Any logic: an empty `{}` infers
            // Dict[Any, Any], and "string" != "Any" made `d["k"] = 1` fail with
            // "Dictionary index expected Any but got string".
            if (indexType != objType.typeArgs[0]) {
                error(expr.index, "Dictionary index expected " + objType.typeArgs[0].toString() + " but got " + indexType.toString());
            }
            if (valType.baseType != "Any" && objType.typeArgs[1].baseType != "Any" && valType != objType.typeArgs[1]) {
                error(expr.value, "Type mismatch in dictionary assignment. Expected " + objType.typeArgs[1].toString() + " but got " + valType.toString());
            }
        }
        return valType;
    }
    
    Environment* env = currentEnv;
    bool found = false;
    while (env) {
        if (env->variables.count(expr.name)) {
            found = true;
            break;
        }
        env = env->enclosing;
    }
    
    if (!found) {
        declareVariable(expr.name, valType);
    } else {
        TypeInfo declaredType = resolveVariable(expr.name);
        if (valType.baseType != "Any" && declaredType.baseType != "Any" && declaredType != valType) {
            error(expr.value,
                  "Type mismatch in variable assignment.", "Expected " + declaredType.toString() + " but got " + valType.toString());
        }
    }
    
    return valType;
}

TypeInfo TypeChecker::checkDestructureAssign(const DestructureAssignExpr& expr) {
    TypeInfo valType = checkExpr(expr.value);
    for (const auto& target : expr.targets) {
        if (!target) {
            continue;
        }
        if (std::holds_alternative<IdentifierExpr*>(target->variant)) {
            std::string name = std::get<IdentifierExpr*>(target->variant)->name;
            Environment* env = currentEnv;
            bool found = false;
            while (env) {
                if (env->variables.count(name)) {
                    found = true;
                    break;
                }
                env = env->enclosing;
            }
            if (!found) {
                declareVariable(name, TypeInfo("Any"));
            }
        } else {
            checkExpr(target);
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
    } else if (expr.op == TokenType::AND || expr.op == TokenType::OR) {
        if (left.baseType != "Any" && left.baseType != "bool") {
            error(expr.left, "Logical operator expected bool but got " + left.toString());
        }
        if (right.baseType != "Any" && right.baseType != "bool") {
            error(expr.right, "Logical operator expected bool but got " + right.toString());
        }
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
    TypeInfo objType = checkExpr(expr.callee);
    if (objType.baseType == "number" || objType.baseType == "string" || 
        objType.baseType == "bool" || objType.baseType == "Array" || 
        objType.baseType == "Dict" || objType.baseType == "nil") {
        error(expr.callee, "Type '" + objType.baseType + "' is not callable.");
    }

    std::vector<TypeInfo> argTypes;
    for (const auto& arg : expr.arguments) argTypes.push_back(checkExpr(arg));
    
    std::string name = "<unknown>";
    FunctionSignature* sig = nullptr;
    
    if (std::holds_alternative<IdentifierExpr*>(expr.callee->variant)) {
        name = std::get<IdentifierExpr*>(expr.callee->variant)->name;
        sig = resolveFunction(name);
    } else if (std::holds_alternative<PropertyAccessExpr*>(expr.callee->variant)) {
        auto propAccess = std::get<PropertyAccessExpr*>(expr.callee->variant);
        TypeInfo objType = checkExpr(propAccess->object);
        
        std::string currentType = objType.baseType;
        if (currentType == "Callable" && std::holds_alternative<IdentifierExpr*>(propAccess->object->variant)) {
            currentType = std::get<IdentifierExpr*>(propAccess->object->variant)->name;
        }

        if (currentType != "Any") {
            name = currentType + "." + propAccess->property;
            sig = resolveFunction(name);
        }
    }
    
    if (sig) {
        FunctionSignature substitutedSig = *sig;
        
        std::unordered_map<std::string, TypeInfo> bindings;
        if (std::holds_alternative<PropertyAccessExpr*>(expr.callee->variant)) {
            auto propAccess = std::get<PropertyAccessExpr*>(expr.callee->variant);
            TypeInfo objType = checkExpr(propAccess->object);
            if (genericParameters.count(objType.baseType)) {
                const auto& names = genericParameters[objType.baseType];
                for (size_t i = 0; i < names.size() && i < objType.typeArgs.size(); i++) {
                    bindings[names[i]] = objType.typeArgs[i];
                }
            }
        } else if (genericParameters.count(name)) {
            // Static function call or constructor with generics
            const auto& names = genericParameters[name];
            // If the user specified type args on the call, e.g. List[String]()
            if (objType.typeArgs.size() > 0) {
                for (size_t i = 0; i < names.size() && i < objType.typeArgs.size(); i++) {
                    bindings[names[i]] = objType.typeArgs[i];
                }
            }
        }
        
        if (!bindings.empty()) {
            for (auto& paramType : substitutedSig.paramTypes) {
                paramType = substituteType(paramType, bindings);
            }
            substitutedSig.returnType = substituteType(substitutedSig.returnType, bindings);
        }
        
        // Only verify arity if not variadic, but for simplicity we skip variadic check here or just check basic args
            // In EZ, builtins aren't in this environment. So `sig` might be null for builtins!
            // If sig exists, we verify it.
            size_t minRequired = substitutedSig.isVariadic ? (substitutedSig.minArgs > 0 ? substitutedSig.minArgs - 1 : 0) : substitutedSig.minArgs;
            
            // Resolve Keyword Arguments
            if (expr.argNames.size() > 0) {
                bool hasKeywords = false;
                for (const auto& kw : expr.argNames) {
                    if (!kw.empty()) { hasKeywords = true; break; }
                }
                if (hasKeywords) {
                    std::vector<ExprPtr> newArgs(substitutedSig.paramTypes.size(), nullptr);
                    std::vector<TypeInfo> newArgTypes(substitutedSig.paramTypes.size(), TypeInfo("Any"));
                    std::vector<bool> provided(substitutedSig.paramTypes.size(), false);
                    
                    for (size_t i = 0; i < expr.arguments.size(); ++i) {
                        if (!expr.argNames[i].empty()) {
                            // Keyword arg
                            auto it = std::find(substitutedSig.paramNames.begin(), substitutedSig.paramNames.end(), expr.argNames[i]);
                            if (it == substitutedSig.paramNames.end()) {
                                error(expr.callee, "Function '" + name + "' has no parameter named '" + expr.argNames[i] + "'.");
                                continue;
                            }
                            size_t idx = std::distance(substitutedSig.paramNames.begin(), it);
                            if (provided[idx]) {
                                error(expr.callee, "Duplicate argument for parameter '" + expr.argNames[i] + "'.");
                            }
                            newArgs[idx] = expr.arguments[i];
                            newArgTypes[idx] = argTypes[i];
                            provided[idx] = true;
                        } else {
                            // Positional arg
                            if (i >= substitutedSig.paramTypes.size()) {
                                error(expr.callee, "Too many arguments provided.");
                                continue;
                            }
                            if (provided[i]) {
                                error(expr.callee, "Positional argument follows keyword argument for the same parameter.");
                            }
                            newArgs[i] = expr.arguments[i];
                            newArgTypes[i] = argTypes[i];
                            provided[i] = true;
                        }
                    }
                    
                    // Filter out nullptrs (optional arguments not provided)
                    std::vector<ExprPtr> finalArgs;
                    std::vector<TypeInfo> finalArgTypes;
                    for (size_t i = 0; i < newArgs.size(); ++i) {
                        if (newArgs[i]) {
                            finalArgs.push_back(newArgs[i]);
                            finalArgTypes.push_back(newArgTypes[i]);
                        } else if (i < minRequired) {
                            error(expr.callee, "Missing required argument '" + substitutedSig.paramNames[i] + "'.");
                        }
                    }
                    
                    // Mutate the AST to reorder the arguments
                    CallExpr* mutExpr = const_cast<CallExpr*>(&expr);
                    mutExpr->arguments = finalArgs;
                    mutExpr->argNames.clear();
                    
                    argTypes = finalArgTypes;
                }
            }
            if (!substitutedSig.isVariadic && (argTypes.size() < minRequired || argTypes.size() > substitutedSig.paramTypes.size())) {
                std::string signatureStr = "Function signature: " + name + "(";
                for (size_t j = 0; j < substitutedSig.paramTypes.size(); ++j) {
                    if (j < substitutedSig.paramNames.size()) signatureStr += substitutedSig.paramNames[j] + ":";
                    signatureStr += substitutedSig.paramTypes[j].toString();
                    if (j + 1 < substitutedSig.paramTypes.size()) signatureStr += ", ";
                }
                signatureStr += ")";
                std::string msg;
                if (minRequired == substitutedSig.paramTypes.size()) {
                    msg = "'" + name + "' expected " + std::to_string(substitutedSig.paramTypes.size()) + " args but got " + std::to_string(argTypes.size());
                } else {
                    msg = "'" + name + "' expected between " + std::to_string(minRequired) + " and " + std::to_string(substitutedSig.paramTypes.size()) + " args but got " + std::to_string(argTypes.size());
                }
                error(expr.callee, msg, signatureStr);
            } else if (substitutedSig.isVariadic && argTypes.size() < minRequired) {
                std::string msg = "'" + name + "' expected at least " + std::to_string(minRequired) + " args but got " + std::to_string(argTypes.size());
                error(expr.callee, msg);
            } else {
                for (size_t i = 0; i < argTypes.size(); i++) {
                    if (argTypes[i] != substitutedSig.paramTypes[i] && argTypes[i].baseType != "Any" && substitutedSig.paramTypes[i].baseType != "Any") {
                        std::string hint = "";
                        if (substitutedSig.paramTypes[i].baseType == "number" && argTypes[i].baseType == "string") {
                            if (std::holds_alternative<LiteralExpr*>(expr.arguments[i]->variant)) {
                                auto lit = std::get<LiteralExpr*>(expr.arguments[i]->variant);
                                if (std::holds_alternative<std::string>(lit->value)) {
                                    std::string strVal = std::get<std::string>(lit->value);
                                    hint = "Did you mean " + strVal + " instead of \"" + strVal + "\"?";
                                }
                            }
                            if (hint.empty()) hint = "Did you mean to pass a number instead of a string?";
                        }
                        
                        std::string signatureStr = "Function signature: " + name + "(";
                        for (size_t j = 0; j < substitutedSig.paramTypes.size(); ++j) {
                            if (j < substitutedSig.paramNames.size()) signatureStr += substitutedSig.paramNames[j] + ":";
                            signatureStr += substitutedSig.paramTypes[j].toString();
                            if (j + 1 < substitutedSig.paramTypes.size()) signatureStr += ", ";
                        }
                        signatureStr += ")";
                        
                        std::string paramNameStr = (i < substitutedSig.paramNames.size()) ? (" '" + substitutedSig.paramNames[i] + "'") : "";
                        std::string msg = "Argument " + std::to_string(i+1) + paramNameStr + " expects " + substitutedSig.paramTypes[i].toString() + " but got " + argTypes[i].toString();
                        
                        error(expr.arguments[i], msg, signatureStr + "\n  Hint: " + hint);
                    }
                }
            }
            return substitutedSig.returnType;
        } else {
        if (expr.argNames.size() > 0) {
            bool hasKeywords = false;
            for (const auto& kw : expr.argNames) {
                if (!kw.empty()) { hasKeywords = true; break; }
            }
            if (hasKeywords) {
                // If it is dynamically typed, we leave expr.argNames intact so the VM can handle it!
            }
        }
    }
    return TypeInfo("Any");
}

TypeInfo TypeChecker::checkIdentifier(const IdentifierExpr& expr) {
    if (currentEnv) {
        Environment* env = currentEnv;
        while (env) {
            if (env->variables.count(expr.name)) return env->variables[expr.name];
            if (env->functions.count(expr.name)) return TypeInfo("Callable");
            env = env->enclosing;
        }
    }
    
    if (hasImports) {
        return TypeInfo("Any");
    }
    
    error(currentExprContext, "Variable '" + expr.name + "' is used before it is defined.");
    return TypeInfo("Any");
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
    TypeInfo objType = checkExpr(expr.object);
    if (objType.baseType == "nil") {
        if (expr.isOptional) return TypeInfo("nil");
        error(currentExprContext, "Cannot access property '" + expr.property + "' on nil.");
        return TypeInfo("Any");
    }
    if (objType.baseType != "Any" && objType.baseType != "Dict") {
        std::string currentType = objType.baseType;
        
        // Handle static methods on classes (e.g. ModelName.load())
        if (currentType == "Callable" && std::holds_alternative<IdentifierExpr*>(expr.object->variant)) {
            currentType = std::get<IdentifierExpr*>(expr.object->variant)->name;
        }

        while (!currentType.empty()) {
            std::string propKey = currentType + "." + expr.property;
            Environment* env = currentEnv;
            while (env) {
                if (env->variables.count(propKey)) {
                    TypeInfo propType = env->variables[propKey];
                    if (genericParameters.count(objType.baseType)) {
                        const auto& names = genericParameters[objType.baseType];
                        std::unordered_map<std::string, TypeInfo> bindings;
                        for (size_t i = 0; i < names.size() && i < objType.typeArgs.size(); i++) {
                            bindings[names[i]] = objType.typeArgs[i];
                        }
                        if (!bindings.empty()) propType = substituteType(propType, bindings);
                    }
                    return propType;
                }
                if (env->functions.count(propKey)) {
                    return TypeInfo("Callable"); // Or Function type
                }
                env = env->enclosing;
            }
            if (modelHierarchy.count(currentType)) {
                currentType = modelHierarchy[currentType];
            } else {
                break;
            }
        }
        if (expr.isOptional) return TypeInfo("Any");
        
        // Dynamic properties are allowed on models/classes
        return TypeInfo("Any");
    }
    return TypeInfo("Any");
}

TypeInfo TypeChecker::checkSet(const SetExpr& expr) {
    checkExpr(expr.object);
    return checkExpr(expr.value);
}

TypeInfo TypeChecker::checkSelf(const SelfExpr& expr) {
    if (currentModel.empty()) {
        error(currentExprContext, "'self' cannot be used outside of a model or class");
        return TypeInfo("Any");
    }
    return TypeInfo(currentModel);
}

TypeInfo TypeChecker::checkSuper(const SuperExpr& expr) {
    if (currentModel.empty()) {
        error(currentExprContext, "'super' cannot be used outside of a model");
        return TypeInfo("Any");
    }
    if (modelHierarchy.count(currentModel) == 0) {
        error(currentExprContext, "Cannot use 'super' in a model with no parent");
        return TypeInfo("Any");
    }
    return TypeInfo(modelHierarchy[currentModel]);
}

TypeInfo TypeChecker::checkNew(const NewExpr& expr) {
    for (const auto& arg : expr.arguments) checkExpr(arg);
    std::vector<TypeInfo> typeArgs;
    for (const auto& ta : expr.typeArgs) typeArgs.push_back(TypeInfo::fromAST(ta));
    return TypeInfo(expr.className, typeArgs);
}

TypeInfo TypeChecker::checkIndex(const IndexExpr& expr) {
    TypeInfo objType = checkExpr(expr.object);
    TypeInfo indexType = checkExpr(expr.index);
    if (objType.baseType == "Array" && objType.typeArgs.size() == 1) {
        if (indexType.baseType != "Any" && indexType.baseType != "number") {
            error(expr.index, "Array index must be a number");
        }
        return objType.typeArgs[0];
    } else if (objType.baseType == "Dict" && objType.typeArgs.size() == 2) {
        // As in checkAssign: operator!= is Any-aware on both sides, unlike the
        // raw baseType string compare this replaced (which made `d["k"]` on an
        // empty `{}` fail with "expected Any but got string").
        if (indexType != objType.typeArgs[0]) {
            error(expr.index, "Dictionary index expected " + objType.typeArgs[0].toString() + " but got " + indexType.toString());
        }
        return objType.typeArgs[1];
    } else if (objType.baseType == "string") {
        if (indexType.baseType != "Any" && indexType.baseType != "number") {
            error(expr.index, "String index must be a number");
        }
        return TypeInfo("string");
    }
    return TypeInfo("Any");
}

TypeInfo TypeChecker::checkArray(const ArrayExpr& expr) {
    if (expr.elements.empty()) return TypeInfo("Array", {TypeInfo("Any")});
    TypeInfo elemType = checkExpr(expr.elements[0]);
    for (size_t i = 1; i < expr.elements.size(); ++i) {
        TypeInfo t = checkExpr(expr.elements[i]);
        if (t.baseType != "Any" && elemType.baseType != "Any" && t != elemType) {
            elemType = TypeInfo("Any");
            break;
        }
    }
    return TypeInfo("Array", {elemType});
}

TypeInfo TypeChecker::checkTuple(const TupleExpr& expr) {
    std::vector<TypeInfo> elementTypes;
    for (const auto& elem : expr.elements) {
        elementTypes.push_back(checkExpr(elem));
    }
    return TypeInfo("Tuple", elementTypes);
}

TypeInfo TypeChecker::checkDictionary(const DictionaryExpr& expr) {
    TypeInfo keyType = TypeInfo("Any");
    TypeInfo valType = TypeInfo("Any");
    bool first = true;
    for (const auto& pair : expr.pairs) {
        TypeInfo k = checkExpr(pair.first);
        TypeInfo v = checkExpr(pair.second);
        if (first) {
            keyType = k;
            valType = v;
            first = false;
        } else {
            if (k.baseType != "Any" && keyType.baseType != "Any" && k != keyType) keyType = TypeInfo("Any");
            if (v.baseType != "Any" && valType.baseType != "Any" && v != valType) valType = TypeInfo("Any");
        }
    }
    return TypeInfo("Dict", {keyType, valType});
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
    TypeInfo awaitedType = checkExpr(expr.expression);
    if ((awaitedType.baseType == "Task" || awaitedType.baseType == "Future") && awaitedType.typeArgs.size() == 1) {
        return awaitedType.typeArgs[0];
    }
    return awaitedType;
}
