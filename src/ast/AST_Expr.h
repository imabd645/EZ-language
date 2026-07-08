#ifndef AST_EXPR_H
#define AST_EXPR_H

#include "AST_Core.h"
#include <optional>

// Literal expression (numbers, strings, booleans, nil)
struct LiteralExpr {
    std::variant<std::nullptr_t, double, long long, std::string, bool> value;
    
    explicit LiteralExpr(std::nullptr_t) : value(nullptr) {}
    explicit LiteralExpr(double val) : value(val) {}
    explicit LiteralExpr(long long val) : value(val) {}
    explicit LiteralExpr(const std::string& val) : value(val) {}
    explicit LiteralExpr(bool val) : value(val) {}
};

// Variable reference
struct IdentifierExpr {
    std::string name;
    
    explicit IdentifierExpr(const std::string& name) : name(name) {}
};

// Binary operation
struct BinaryExpr {
    ExprPtr left;
    TokenType op;
    ExprPtr right;
    
    BinaryExpr(ExprPtr left, TokenType op, ExprPtr right)
        : left(std::move(left)), op(op), right(std::move(right)) {}
};

// Unary operation
struct UnaryExpr {
    TokenType op;
    ExprPtr operand;
    
    UnaryExpr(TokenType op, ExprPtr operand)
        : op(op), operand(std::move(operand)) {}
};

// Function call
struct CallExpr {
    ExprPtr callee;
    std::vector<ExprPtr> arguments;
    std::vector<std::string> argNames;
    bool isTailCall = false;
    
    CallExpr(ExprPtr callee, std::vector<ExprPtr> args, std::vector<std::string> names = {})
        : callee(std::move(callee)), arguments(std::move(args)), argNames(std::move(names)) {}
};

// Array/string indexing
struct IndexExpr {
    ExprPtr object;
    ExprPtr index;
    
    IndexExpr(ExprPtr obj, ExprPtr idx)
        : object(std::move(obj)), index(std::move(idx)) {}
};

// Array literal
struct ArrayExpr {
    std::vector<ExprPtr> elements;
    
    explicit ArrayExpr(std::vector<ExprPtr> elements) 
        : elements(std::move(elements)) {}
};

// Tuple literal expression (expr, expr)
struct TupleExpr {
    std::vector<ExprPtr> elements;
    
    explicit TupleExpr(std::vector<ExprPtr> elements) 
        : elements(std::move(elements)) {}
};

// Assignment expression
struct AssignExpr {
    std::string name; // Still kept for simple variable assignment optimizations
    ExprPtr value;
    ExprPtr index;    // For indexed assignment (arr[i] = val)
    ExprPtr object;   // For complex indexed assignment (obj.prop[i] = val)
    std::optional<TokenType> compoundOp;
    
    AssignExpr(const std::string& name, ExprPtr value, ExprPtr index = nullptr, ExprPtr object = nullptr, std::optional<TokenType> compoundOp = std::nullopt)
        : name(name), value(std::move(value)), index(std::move(index)), object(std::move(object)), compoundOp(compoundOp) {}
};

struct LogicalExpr {
    ExprPtr left;
    TokenType op;
    ExprPtr right;
    
    LogicalExpr(ExprPtr left, TokenType op, ExprPtr right)
        : left(std::move(left)), op(op), right(std::move(right)) {}
};

// Lambda expression (anonymous function)
struct LambdaExpr {
    std::vector<std::string> params;
    std::vector<TypeASTPtr> paramTypes;
    ExprPtr body;  // Expression body for single-expression lambdas
    std::vector<StmtPtr> stmtBody;  // Statement body for multi-statement lambdas
    TypeASTPtr returnType;
    bool isVariadic = false;
    bool isAsync = false;
    
    LambdaExpr(std::vector<std::string> params, std::vector<TypeASTPtr> paramTypes, ExprPtr body, TypeASTPtr returnType = nullptr, bool variadic = false, bool isAsync = false)
        : params(std::move(params)), paramTypes(std::move(paramTypes)), body(std::move(body)), returnType(std::move(returnType)), isVariadic(variadic), isAsync(isAsync) {}
    
    LambdaExpr(std::vector<std::string> params, std::vector<TypeASTPtr> paramTypes, std::vector<StmtPtr> stmtBody, TypeASTPtr returnType = nullptr, bool variadic = false, bool isAsync = false)
        : params(std::move(params)), paramTypes(std::move(paramTypes)), body(nullptr), stmtBody(std::move(stmtBody)), returnType(std::move(returnType)), isVariadic(variadic), isAsync(isAsync) {}
};

// Property access expression (self.name or obj.property)
struct PropertyAccessExpr {
    ExprPtr object;
    std::string property;
    bool isOptional = false;
    
    PropertyAccessExpr(ExprPtr obj, const std::string& prop, bool optional = false)
        : object(std::move(obj)), property(prop), isOptional(optional) {}
};

// Self reference expression
struct SelfExpr {};

// Super reference expression
struct SuperExpr {};

// New instance creation expression (model instantiation)
struct NewExpr {
    std::string className;
    std::vector<ExprPtr> arguments;
    std::vector<TypeASTPtr> typeArgs;
    
    NewExpr(const std::string& name, std::vector<ExprPtr> args, std::vector<TypeASTPtr> tArgs = {})
        : className(name), arguments(std::move(args)), typeArgs(std::move(tArgs)) {}
};

// Property assignment expression (object.property = value)
struct SetExpr {
    ExprPtr object;
    std::string name;
    ExprPtr value;
    std::optional<TokenType> compoundOp;
    
    SetExpr(ExprPtr obj, const std::string& name, ExprPtr val, std::optional<TokenType> compoundOp = std::nullopt)
        : object(std::move(obj)), name(name), value(std::move(val)), compoundOp(compoundOp) {}
};

// Dictionary literal expression { key: value, ... }
struct DictionaryExpr {
    std::vector<std::pair<ExprPtr, ExprPtr>> pairs;
    
    explicit DictionaryExpr(std::vector<std::pair<ExprPtr, ExprPtr>> pairs) 
        : pairs(std::move(pairs)) {}
};

// Spread expression (...expr)
struct SpreadExpr {
    ExprPtr expression;
    
    explicit SpreadExpr(ExprPtr expr) : expression(std::move(expr)) {}
};

// Ternary operator expression (cond ? then : else)
struct TernaryExpr {
    ExprPtr condition;
    ExprPtr thenBranch;
    ExprPtr elseBranch;
    
    TernaryExpr(ExprPtr cond, ExprPtr thenBr, ExprPtr elseBr)
        : condition(std::move(cond)), thenBranch(std::move(thenBr)), elseBranch(std::move(elseBr)) {}
};

// Await expression (await expr)
struct AwaitExpr {
    ExprPtr expression;
    
    explicit AwaitExpr(ExprPtr expr) : expression(std::move(expr)) {}
};

struct DestructureAssignExpr {
    std::vector<ExprPtr> targets;
    ExprPtr value;
    
    DestructureAssignExpr(std::vector<ExprPtr> targets, ExprPtr value)
        : targets(std::move(targets)), value(std::move(value)) {}
};


#endif // AST_EXPR_H
