#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <memory>
#include <vector>
#include <string>
#include <iostream>
#include "AST.h"
#include "Value.h"
#include "Environment.h"
#include "GC.h"

// Control flow exceptions
class ReturnException : public std::exception {
public:
    Value value;
    ReturnException(const Value& val = Value()) : value(val) {}
};

class BreakException : public std::exception {};
class ContinueException : public std::exception {};

class Interpreter {
public:
    Interpreter();
    explicit Interpreter(std::shared_ptr<Environment> startEnv);
    
    void interpret(const std::vector<StmtPtr>& statements);
    Value evaluate(const ExprPtr& expr);
    void execute(const StmtPtr& stmt);
    
    // Public for native functions
    std::shared_ptr<Environment> getGlobalEnv() const { return globalEnv; }
    std::shared_ptr<Environment> getCurrentEnv() const { return currentEnv; }
    void setCurrentEnv(std::shared_ptr<Environment> env) { currentEnv = env; }
    void setGlobalEnv(std::shared_ptr<Environment> env) { globalEnv = env; currentEnv = env; }
    
    // For REPL mode
    Value evaluateExpression(const ExprPtr& expr) { return evaluate(expr); }
    
    // For calling functions from native code
    Value callFunction(const Value& callee, const std::vector<Value>& args, int line);
    
    // Define global variable (for built-ins)
    void defineGlobal(const std::string& name, const Value& value);
    
private:
    std::shared_ptr<Environment> globalEnv;
    std::shared_ptr<Environment> currentEnv;
    
    // Initialization
    void initBuiltins();
    
    // Expression evaluation
    Value visitLiteral(const std::shared_ptr<LiteralExpr>& expr);
    Value visitIdentifier(const std::shared_ptr<IdentifierExpr>& expr, int line);
    Value visitBinary(const std::shared_ptr<BinaryExpr>& expr, int line);
    Value visitUnary(const std::shared_ptr<UnaryExpr>& expr, int line);
    Value visitCall(const std::shared_ptr<CallExpr>& expr, int line);
    Value visitIndex(const std::shared_ptr<IndexExpr>& expr, int line);
    Value visitArray(const std::shared_ptr<ArrayExpr>& expr, int line);
    Value visitAssign(const std::shared_ptr<AssignExpr>& expr, int line);
    Value visitLogical(const std::shared_ptr<LogicalExpr>& expr, int line);
    Value visitLambda(const std::shared_ptr<LambdaExpr>& expr, int line);
    Value visitPropertyAccess(const std::shared_ptr<PropertyAccessExpr>& expr, int line);
    Value visitSelf(const std::shared_ptr<SelfExpr>& expr, int line);
    Value visitNew(const std::shared_ptr<NewExpr>& expr, int line);
    Value visitSet(const std::shared_ptr<SetExpr>& expr, int line);
    Value visitDictionary(const std::shared_ptr<DictionaryExpr>& expr, int line);
    
    // Statement execution
    void visitExprStmt(const std::shared_ptr<ExprStmt>& stmt);
    void visitOutStmt(const std::shared_ptr<OutStmt>& stmt);
    void visitVarDeclStmt(const std::shared_ptr<VarDeclStmt>& stmt);
    void visitBlockStmt(const std::shared_ptr<BlockStmt>& stmt);
    void visitWhenStmt(const std::shared_ptr<WhenStmt>& stmt);
    void visitWhileStmt(const std::shared_ptr<WhileStmt>& stmt);
    void visitRepeatStmt(const std::shared_ptr<RepeatStmt>& stmt);
    void visitGetStmt(const std::shared_ptr<GetStmt>& stmt);
    void visitTaskStmt(const std::shared_ptr<TaskStmt>& stmt);
    void visitGiveStmt(const std::shared_ptr<GiveStmt>& stmt);
    void visitEscapeStmt(const std::shared_ptr<EscapeStmt>& stmt);
    void visitSkipStmt(const std::shared_ptr<SkipStmt>& stmt);
    void visitModelStmt(const std::shared_ptr<ModelStmt>& stmt);
    void visitStructStmt(const std::shared_ptr<StructStmt>& stmt);
    void visitUseStmt(const std::shared_ptr<UseStmt>& stmt);
    void visitTryStmt(const std::shared_ptr<TryStmt>& stmt);
    void visitThrowStmt(const std::shared_ptr<ThrowStmt>& stmt);
    
    // Helpers
    void executeBlock(const std::vector<StmtPtr>& statements, std::shared_ptr<Environment> env);
    void checkNumberOperand(TokenType op, const Value& operand, int line);
    void checkNumberOperands(TokenType op, const Value& left, const Value& right, int line);
};

#endif // INTERPRETER_H
