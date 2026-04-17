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

class TailCallException : public std::exception {
public:
    Value callee;
    std::vector<Value> args;
    TailCallException(const Value& c, const std::vector<Value>& a) 
        : callee(c), args(a) {}
};

struct CallFrame {
    std::string functionName;
    std::string filename;
    int line;
};

class Interpreter {
public:
    Interpreter();
    explicit Interpreter(std::shared_ptr<Environment> startEnv);
    
    void interpret(const std::vector<StmtPtr>& statements);
    void runtimeError(const std::string& message, int line, const std::string& filename);
    Value evaluate(const ExprPtr& expr);
    void execute(const StmtPtr& stmt);
    
    // Public for native functions
    std::shared_ptr<Environment> getGlobalEnv() const { return globalEnv; }
    std::shared_ptr<Environment> getCurrentEnv() const { return currentEnv; }
    void setCurrentEnv(std::shared_ptr<Environment> env) { currentEnv = env; }
    void setGlobalEnv(std::shared_ptr<Environment> env) { globalEnv = env; currentEnv = env; }
    
    // For REPL mode
    Value evaluateExpression(const ExprPtr& expr) { return evaluate(expr); }
    
    // For string conversion (supports toString override)
    std::string stringify(const Value& val, int line = 0, const std::string& filename = "");
    
    // For calling functions from native code
    Value callFunction(const Value& callee, const std::vector<Value>& args, int line, const std::string& filename);
    
    // Define global variable (for built-ins)
    void defineGlobal(const std::string& name, const Value& value);
    
    // Print current stack trace
    void printStackTrace() const;
    
private:
    std::shared_ptr<Environment> globalEnv;
    std::shared_ptr<Environment> currentEnv;
    std::vector<CallFrame> callStack;
    std::unordered_map<std::string, std::vector<std::string>> definedInterfaces;
    int callDepth = 0;
    static constexpr int MAX_CALL_DEPTH = 500;
    
    // Saved environment stack — these are GC roots
    std::vector<std::shared_ptr<Environment>> envStack;
    
    // Initialization
    void initBuiltins();
    
    // Expression evaluation
    Value visitLiteral(const std::shared_ptr<LiteralExpr>& expr);
    Value visitIdentifier(const std::shared_ptr<IdentifierExpr>& expr, int line, const std::string& filename);
    Value visitBinary(const std::shared_ptr<BinaryExpr>& expr, int line, const std::string& filename);
    Value visitUnary(const std::shared_ptr<UnaryExpr>& expr, int line, const std::string& filename);
    Value visitCall(const std::shared_ptr<CallExpr>& expr, int line, const std::string& filename);
    Value visitIndex(const std::shared_ptr<IndexExpr>& expr, int line, const std::string& filename);
    Value visitArray(const std::shared_ptr<ArrayExpr>& expr, int line, const std::string& filename);
    Value visitAssign(const std::shared_ptr<AssignExpr>& expr, int line, const std::string& filename);
    Value visitLogical(const std::shared_ptr<LogicalExpr>& expr, int line, const std::string& filename);
    Value visitTernary(const std::shared_ptr<TernaryExpr>& expr, int line, const std::string& filename);
    Value visitLambda(const std::shared_ptr<LambdaExpr>& expr, int line, const std::string& filename);
    Value visitPropertyAccess(const std::shared_ptr<PropertyAccessExpr>& expr, int line, const std::string& filename);
    Value visitSelf(const std::shared_ptr<SelfExpr>& expr, int line, const std::string& filename);
    Value visitNew(const std::shared_ptr<NewExpr>& expr, int line, const std::string& filename);
    Value visitSet(const std::shared_ptr<SetExpr>& expr, int line, const std::string& filename);
    Value visitDictionary(const std::shared_ptr<DictionaryExpr>& expr, int line, const std::string& filename);
    
    // Statement execution
    void visitExprStmt(const std::shared_ptr<ExprStmt>& stmt);
    void visitOutStmt(const std::shared_ptr<OutStmt>& stmt);
    void visitVarDeclStmt(const std::shared_ptr<VarDeclStmt>& stmt, int line, const std::string& filename);
    void visitBlockStmt(const std::shared_ptr<BlockStmt>& stmt);
    void visitWhenStmt(const std::shared_ptr<WhenStmt>& stmt, int line, const std::string& filename);
    void visitWhileStmt(const std::shared_ptr<WhileStmt>& stmt, int line, const std::string& filename);
    void visitRepeatStmt(const std::shared_ptr<RepeatStmt>& stmt, int line, const std::string& filename);
    void visitGetStmt(const std::shared_ptr<GetStmt>& stmt, int line, const std::string& filename);
    void visitTaskStmt(const std::shared_ptr<TaskStmt>& stmt, int line, const std::string& filename);
    void visitGiveStmt(const std::shared_ptr<GiveStmt>& stmt, int line, const std::string& filename);
    void visitEscapeStmt(const std::shared_ptr<EscapeStmt>& stmt, int line, const std::string& filename);
    void visitSkipStmt(const std::shared_ptr<SkipStmt>& stmt, int line, const std::string& filename);
    void visitModelStmt(const std::shared_ptr<ModelStmt>& stmt, int line, const std::string& filename);
    void visitStaticStmt(const std::shared_ptr<StaticStmt>& stmt, int line, const std::string& filename);
    void visitInterfaceStmt(const std::shared_ptr<InterfaceStmt>& stmt, int line, const std::string& filename);
    void visitStructStmt(const std::shared_ptr<StructStmt>& stmt, int line, const std::string& filename);
    void visitUseStmt(const std::shared_ptr<UseStmt>& stmt, int line, const std::string& filename);
    void visitTryStmt(const std::shared_ptr<TryStmt>& stmt, int line, const std::string& filename);
    void visitThrowStmt(const std::shared_ptr<ThrowStmt>& stmt, int line, const std::string& filename);
    
    // Helpers
    void executeBlock(const std::vector<StmtPtr>& statements, std::shared_ptr<Environment> env);
    void checkNumberOperand(TokenType op, const Value& operand, int line, const std::string& filename);
    void checkNumberOperands(TokenType op, const Value& left, const Value& right, int line, const std::string& filename);
    
    // Operator Overloading
    Value lookupAndCallBinaryOperator(const Value& left, const Value& right, const std::string& op, int line, const std::string& filename, bool& handled);
    Value lookupAndCallUnaryOperator(const Value& operand, const std::string& op, int line, const std::string& filename, bool& handled);
};

#endif // INTERPRETER_H
