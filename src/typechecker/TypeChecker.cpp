#include "typechecker/TypeChecker.h"
#include <iostream>
#include <fstream>
#include <functional>
#include <algorithm>

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

void TypeChecker::warn(const ExprPtr& expr, const std::string& message, const std::string& hint) {
    if (expr) warn(expr->line, expr->column, expr->length, expr->filename, message, hint);
    else warn(0, 0, 0, "", message, hint);
}

void TypeChecker::warn(const StmtPtr& stmt, const std::string& message, const std::string& hint) {
    if (stmt) warn(stmt->line, stmt->column, stmt->length, stmt->filename, message, hint);
    else warn(0, 0, 0, "", message, hint);
}

void TypeChecker::warn(int line, int column, int length, const std::string& filename, const std::string& message, const std::string& hint) {
    std::cerr << "\033[33mType Warning\033[0m at line " << line << ", column " << column << ":\n\n";

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

void TypeChecker::warn(int line, const std::string& message) {
    warn(line, 0, 0, "", message, "");
}

TypeInfo TypeChecker::substituteType(const TypeInfo& type, const std::unordered_map<std::string, TypeInfo>& bindings) {
    if (bindings.count(type.baseType)) {
        TypeInfo res = bindings.at(type.baseType);
        // If the original type also had type args, we might need to preserve them or substitute them.
        // For standard generics, `T` won't have type args. 
        return res;
    }
    
    TypeInfo result = type;
    for (size_t i = 0; i < result.typeArgs.size(); i++) {
        result.typeArgs[i] = substituteType(result.typeArgs[i], bindings);
    }
    return result;
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

bool TypeChecker::check(const std::vector<StmtPtr>& statements, const std::vector<std::string>& builtins) {
    hadError = false;
    hasImports = false;
    currentEnv = new Environment();
    
    for (const auto& builtin : builtins) {
        declareVariable(builtin, TypeInfo("Task"));
    }
    
    // First pass: declare all functions
    for (const auto& stmt : statements) {
        if (std::holds_alternative<std::shared_ptr<TaskStmt>>(stmt->variant)) {
            auto task = std::get<std::shared_ptr<TaskStmt>>(stmt->variant);
            FunctionSignature sig;
            for (const auto& p : task->params) sig.paramNames.push_back(p);
            for (const auto& t : task->paramTypes) sig.paramTypes.push_back(TypeInfo::fromAST(t));
            sig.returnType = TypeInfo::fromAST(task->returnType);
            sig.isVariadic = task->isVariadic;
            size_t minArgs = 0;
            for (const auto& dv : task->defaultValues) {
                if (!dv) minArgs++;
            }
            sig.minArgs = minArgs;
            
            declareFunction(task->name, sig);
        } else if (std::holds_alternative<std::shared_ptr<ModelStmt>>(stmt->variant)) {
            auto model = std::get<std::shared_ptr<ModelStmt>>(stmt->variant);
            FunctionSignature sig;
            for (const auto& p : model->initParams) sig.paramNames.push_back(p);
            for (const auto& t : model->initParamTypes) sig.paramTypes.push_back(TypeInfo::fromAST(t));
            sig.returnType = TypeInfo(model->name);
            size_t initMinArgs = 0;
            for (const auto& dv : model->initDefaultValues) {
                if (!dv) initMinArgs++;
            }
            sig.minArgs = initMinArgs;
            declareFunction(model->name, sig);
            
            // Register init as a callable method for super.init()
            FunctionSignature initSig;
            for (const auto& p : model->initParams) initSig.paramNames.push_back(p);
            for (const auto& t : model->initParamTypes) initSig.paramTypes.push_back(TypeInfo::fromAST(t));
            initSig.returnType = TypeInfo("Any");
            initSig.minArgs = initMinArgs;
            declareFunction(model->name + ".init", initSig);
            
            // Register .load() for persistent models
            if (!model->persistPath.empty()) {
                FunctionSignature loadSig;
                loadSig.returnType = TypeInfo(model->name);
                declareFunction(model->name + ".load", loadSig);
            }
            
            // Register explicit member declarations
            for (const auto& member : model->members) {
                if (member.isMethod) {
                    FunctionSignature methodSig;
                    for (const auto& p : member.params) methodSig.paramNames.push_back(p);
                    for (const auto& t : member.paramTypes) methodSig.paramTypes.push_back(TypeInfo::fromAST(t));
                    methodSig.returnType = TypeInfo::fromAST(member.typeHint);
                    size_t methodMinArgs = 0;
                    for (const auto& dv : member.defaultValues) {
                        if (!dv) methodMinArgs++;
                    }
                    methodSig.minArgs = methodMinArgs;
                    declareFunction(model->name + "." + member.name, methodSig);
                } else {
                    TypeInfo propType = TypeInfo::fromAST(member.typeHint);
                    declareVariable(model->name + "." + member.name, propType);
                }
            }

            // Register self.xxx properties from init body and all method bodies
            // so that checkPropertyAccess can resolve them at type-check time.
            std::function<void(const std::vector<StmtPtr>&)> scanBodyForSelfProps =
                [&](const std::vector<StmtPtr>& body) {
                    for (const auto& s : body) {
                        if (!s) continue;
                        // ExpressionStmt wrapping a SetExpr (self.xxx = ...)
                        if (std::holds_alternative<std::shared_ptr<ExpressionStmt>>(s->variant)) {
                            auto exprStmt = std::get<std::shared_ptr<ExpressionStmt>>(s->variant);
                            if (exprStmt->expr &&
                                std::holds_alternative<std::shared_ptr<SetExpr>>(exprStmt->expr->variant)) {
                                auto setExpr = std::get<std::shared_ptr<SetExpr>>(exprStmt->expr->variant);
                                if (setExpr->object &&
                                    std::holds_alternative<std::shared_ptr<SelfExpr>>(setExpr->object->variant)) {
                                    std::string key = model->name + "." + setExpr->name;
                                    if (!currentEnv->variables.count(key))
                                        declareVariable(key, TypeInfo("Any"));
                                }
                            }
                        }
                        // Also recurse into block bodies (when/while/etc.)
                        if (std::holds_alternative<std::shared_ptr<BlockStmt>>(s->variant)) {
                            scanBodyForSelfProps(std::get<std::shared_ptr<BlockStmt>>(s->variant)->statements);
                        } else if (std::holds_alternative<std::shared_ptr<WhenStmt>>(s->variant)) {
                            auto ws = std::get<std::shared_ptr<WhenStmt>>(s->variant);
                            if (ws->thenBranch) scanBodyForSelfProps({ws->thenBranch});
                            if (ws->elseBranch) scanBodyForSelfProps({ws->elseBranch});
                        }
                    }
                };

            // Scan init body
            scanBodyForSelfProps(model->initBody);

            // Scan all method bodies too (self.xxx set in non-init methods)
            for (const auto& member : model->members) {
                if (member.isMethod) {
                    scanBodyForSelfProps(member.body);
                }
            }

        } else if (std::holds_alternative<std::shared_ptr<StructStmt>>(stmt->variant)) {
            auto structStmt = std::get<std::shared_ptr<StructStmt>>(stmt->variant);
            FunctionSignature sig;
            for (size_t i = 0; i < structStmt->fields.size(); ++i) {
                sig.paramNames.push_back(structStmt->fields[i]);
                TypeInfo fieldType = TypeInfo::fromAST(structStmt->types[i]);
                sig.paramTypes.push_back(fieldType);
                declareVariable(structStmt->name + "." + structStmt->fields[i], fieldType);
            }
            sig.returnType = TypeInfo(structStmt->name);
            declareFunction(structStmt->name, sig);
        } else if (std::holds_alternative<std::shared_ptr<InterfaceStmt>>(stmt->variant)) {
            auto interfaceStmt = std::get<std::shared_ptr<InterfaceStmt>>(stmt->variant);
            for (const auto& method : interfaceStmt->methods) {
                FunctionSignature methodSig;
                for (const auto& p : method.params) methodSig.paramNames.push_back(p);
                for (const auto& t : method.paramTypes) methodSig.paramTypes.push_back(TypeInfo::fromAST(t));
                methodSig.returnType = TypeInfo::fromAST(method.returnType);
                declareFunction(interfaceStmt->name + "." + method.name, methodSig);
            }
        } else if (std::holds_alternative<std::shared_ptr<ExportStmt>>(stmt->variant)) {
            auto exportStmt = std::get<std::shared_ptr<ExportStmt>>(stmt->variant);
            std::vector<StmtPtr> innerVec;
            innerVec.push_back(exportStmt->inner);
            // Recursively process the inner statement for declarations
            auto tempEnv = currentEnv; // Keep environment
            std::vector<StmtPtr> tempStatements = innerVec;
            for (const auto& innerStmt : tempStatements) {
                if (std::holds_alternative<std::shared_ptr<TaskStmt>>(innerStmt->variant)) {
                    auto task = std::get<std::shared_ptr<TaskStmt>>(innerStmt->variant);
                    FunctionSignature sig;
                    for (const auto& p : task->params) sig.paramNames.push_back(p);
                    for (const auto& t : task->paramTypes) sig.paramTypes.push_back(TypeInfo::fromAST(t));
                    sig.returnType = TypeInfo::fromAST(task->returnType);
                    sig.isVariadic = task->isVariadic;
                    declareFunction(task->name, sig);
                } else if (std::holds_alternative<std::shared_ptr<ModelStmt>>(innerStmt->variant)) {
                    auto model = std::get<std::shared_ptr<ModelStmt>>(innerStmt->variant);
                    FunctionSignature sig;
                    for (const auto& p : model->initParams) sig.paramNames.push_back(p);
                    for (const auto& t : model->initParamTypes) sig.paramTypes.push_back(TypeInfo::fromAST(t));
                    sig.returnType = TypeInfo(model->name);
                    declareFunction(model->name, sig);
                    
                    for (const auto& member : model->members) {
                        if (member.isMethod) {
                            FunctionSignature methodSig;
                            for (const auto& p : member.params) methodSig.paramNames.push_back(p);
                            for (const auto& t : member.paramTypes) methodSig.paramTypes.push_back(TypeInfo::fromAST(t));
                            methodSig.returnType = TypeInfo::fromAST(member.typeHint);
                            declareFunction(model->name + "." + member.name, methodSig);
                        } else {
                            TypeInfo propType = TypeInfo::fromAST(member.typeHint);
                            declareVariable(model->name + "." + member.name, propType);
                        }
                    }

                    std::function<void(const std::vector<StmtPtr>&)> scanBodyForSelfProps =
                        [&](const std::vector<StmtPtr>& body) {
                            for (const auto& s : body) {
                                if (!s) continue;
                                if (std::holds_alternative<std::shared_ptr<ExpressionStmt>>(s->variant)) {
                                    auto exprStmt = std::get<std::shared_ptr<ExpressionStmt>>(s->variant);
                                    if (exprStmt->expr &&
                                        std::holds_alternative<std::shared_ptr<SetExpr>>(exprStmt->expr->variant)) {
                                        auto setExpr = std::get<std::shared_ptr<SetExpr>>(exprStmt->expr->variant);
                                        if (setExpr->object &&
                                            std::holds_alternative<std::shared_ptr<SelfExpr>>(setExpr->object->variant)) {
                                            std::string key = model->name + "." + setExpr->name;
                                            if (!currentEnv->variables.count(key))
                                                declareVariable(key, TypeInfo("Any"));
                                        }
                                    }
                                }
                                if (std::holds_alternative<std::shared_ptr<BlockStmt>>(s->variant)) {
                                    scanBodyForSelfProps(std::get<std::shared_ptr<BlockStmt>>(s->variant)->statements);
                                } else if (std::holds_alternative<std::shared_ptr<WhenStmt>>(s->variant)) {
                                    auto ws = std::get<std::shared_ptr<WhenStmt>>(s->variant);
                                    if (ws->thenBranch) scanBodyForSelfProps({ws->thenBranch});
                                    if (ws->elseBranch) scanBodyForSelfProps({ws->elseBranch});
                                }
                            }
                        };
                    scanBodyForSelfProps(model->initBody);
                    for (const auto& member : model->members) {
                        if (member.isMethod) scanBodyForSelfProps(member.body);
                    }
                }
            }
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

