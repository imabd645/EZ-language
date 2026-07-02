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
    std::vector<std::string> paramNames;
    std::vector<TypeInfo> paramTypes;
    TypeInfo returnType;
    bool isVariadic = false;
    size_t minArgs = 0;
};

class TypeChecker {
public:
    TypeChecker();
    bool check(const std::vector<StmtPtr>& statements, const std::vector<std::string>& builtins = {});
    
private:
    struct Environment {
        std::unordered_map<std::string, TypeInfo> variables;
        std::unordered_map<std::string, FunctionSignature> functions;
        Environment* enclosing;
        
        Environment(Environment* enclosing = nullptr) : enclosing(enclosing) {}
    };
    
    Environment* currentEnv;
    TypeInfo currentReturnType = TypeInfo("Any");
    std::string currentModel; // Tracks enclosing model for 'self'
    ExprPtr currentExprContext; // Tracks current expression for error reporting
    int loopDepth = 0;
    bool hadError = false;
    bool hasImports = false;
    std::unordered_map<std::string, std::string> modelHierarchy;
    std::unordered_map<std::string, std::vector<std::string>> genericParameters;
    
    void beginScope();
    void endScope();
    void declareVariable(const std::string& name, const TypeInfo& type);
    TypeInfo resolveVariable(const std::string& name);
    void declareFunction(const std::string& name, const FunctionSignature& sig);
    FunctionSignature* resolveFunction(const std::string& name);
    
    TypeInfo substituteType(const TypeInfo& type, const std::unordered_map<std::string, TypeInfo>& bindings);
    
    void error(const ExprPtr& expr, const std::string& message, const std::string& hint = "");
    void error(const StmtPtr& stmt, const std::string& message, const std::string& hint = "");
    void error(int line, int column, int length, const std::string& filename, const std::string& message, const std::string& hint = "");
    void error(int line, const std::string& message);

    void warn(const ExprPtr& expr, const std::string& message, const std::string& hint = "");
    void warn(const StmtPtr& stmt, const std::string& message, const std::string& hint = "");
    void warn(int line, int column, int length, const std::string& filename, const std::string& message, const std::string& hint = "");
    void warn(int line, const std::string& message);
    
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
    void checkModel(const ModelStmt& stmt);
    void checkStruct(const StructStmt& stmt);
    void checkInterface(const InterfaceStmt& stmt);
    void checkTry(const TryStmt& stmt);
    void checkThrow(const ThrowStmt& stmt);
    void checkMatch(const MatchStmt& stmt);
    void checkStatic(const StaticStmt& stmt);
    
    // Expressions
    TypeInfo checkExpr(const ExprPtr& expr);
    TypeInfo checkAssign(const AssignExpr& expr);
    TypeInfo checkDestructureAssign(const DestructureAssignExpr& expr);
    TypeInfo checkBinary(const BinaryExpr& expr);
    TypeInfo checkUnary(const UnaryExpr& expr);
    TypeInfo checkCall(const CallExpr& expr);
    TypeInfo checkIdentifier(const IdentifierExpr& expr);
    TypeInfo checkLiteral(const LiteralExpr& expr);
    TypeInfo checkLambda(const LambdaExpr& expr);
    TypeInfo checkPropertyAccess(const PropertyAccessExpr& expr);
    TypeInfo checkSet(const SetExpr& expr);
    TypeInfo checkSelf(const SelfExpr& expr);
    TypeInfo checkSuper(const SuperExpr& expr);
    TypeInfo checkNew(const NewExpr& expr);
    TypeInfo checkIndex(const IndexExpr& expr);
    TypeInfo checkArray(const ArrayExpr& expr);
    TypeInfo checkTuple(const TupleExpr& expr);
    TypeInfo checkDictionary(const DictionaryExpr& expr);
    TypeInfo checkSpread(const SpreadExpr& expr);
    TypeInfo checkTernary(const TernaryExpr& expr);
    TypeInfo checkAwait(const AwaitExpr& expr);
};

#endif // TYPE_CHECKER_H
