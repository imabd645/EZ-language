#ifndef TYPE_CHECKER_H
#define TYPE_CHECKER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "AST.h"

struct TypeInfo {
    std::string baseType;
    std::vector<TypeInfo> typeArgs;
    
    TypeInfo() : baseType("Any") {}
    TypeInfo(const std::string& base) : baseType(base) {}
    TypeInfo(const std::string& base, std::vector<TypeInfo> args) : baseType(base), typeArgs(std::move(args)) {}
    
    bool operator==(const TypeInfo& other) const {
        if (baseType == "Any" || other.baseType == "Any") return true;
        if (baseType != other.baseType) return false;
        if (typeArgs.size() != other.typeArgs.size()) return false;
        for (size_t i = 0; i < typeArgs.size(); i++) {
            if (!(typeArgs[i] == other.typeArgs[i])) return false;
        }
        return true;
    }
    
    bool operator!=(const TypeInfo& other) const {
        return !(*this == other);
    }
    
    std::string toString() const {
        if (typeArgs.empty()) return baseType;
        std::string s = baseType + "[";
        for (size_t i = 0; i < typeArgs.size(); ++i) {
            s += typeArgs[i].toString();
            if (i + 1 < typeArgs.size()) s += ", ";
        }
        s += "]";
        return s;
    }
    
    static TypeInfo fromAST(const TypeASTPtr& ast) {
        if (!ast) return TypeInfo("Any");
        std::vector<TypeInfo> args;
        for (const auto& a : ast->typeArgs) args.push_back(fromAST(a));
        return TypeInfo(ast->baseType, args);
    }
};

struct FunctionSignature {
    std::vector<TypeInfo> paramTypes;
    TypeInfo returnType;
};

class TypeChecker {
public:
    TypeChecker();
    bool check(const std::vector<StmtPtr>& statements);
    
private:
    struct Environment {
        std::unordered_map<std::string, TypeInfo> variables;
        std::unordered_map<std::string, FunctionSignature> functions;
        Environment* enclosing;
        
        Environment(Environment* enclosing = nullptr) : enclosing(enclosing) {}
    };
    
    Environment* currentEnv;
    TypeInfo currentReturnType;
    bool hadError;
    
    void beginScope();
    void endScope();
    void declareVariable(const std::string& name, const TypeInfo& type);
    TypeInfo resolveVariable(const std::string& name);
    void declareFunction(const std::string& name, const FunctionSignature& sig);
    FunctionSignature* resolveFunction(const std::string& name);
    
    void error(int line, const std::string& message);
    
    // Statements
    void checkStmt(const StmtPtr& stmt);
    void checkVarDecl(const VarDeclStmt& stmt);
    void checkTask(const TaskStmt& stmt);
    void checkGive(const GiveStmt& stmt);
    void checkBlock(const BlockStmt& stmt);
    void checkWhen(const WhenStmt& stmt);
    void checkWhile(const WhileStmt& stmt);
    void checkRepeat(const RepeatStmt& stmt);
    void checkGet(const GetStmt& stmt);
    
    // Expressions
    TypeInfo checkExpr(const ExprPtr& expr);
    TypeInfo checkAssign(const AssignExpr& expr);
    TypeInfo checkBinary(const BinaryExpr& expr);
    TypeInfo checkUnary(const UnaryExpr& expr);
    TypeInfo checkCall(const CallExpr& expr);
    TypeInfo checkIdentifier(const IdentifierExpr& expr);
    TypeInfo checkLiteral(const LiteralExpr& expr);
};

#endif // TYPE_CHECKER_H
