#ifndef AST_H
#define AST_H

#include <memory>
#include <vector>
#include <string>
#include <variant>
#include "Token.h"

// Forward declarations
struct Expr;
struct Stmt;

using ExprPtr = std::shared_ptr<Expr>;
using StmtPtr = std::shared_ptr<Stmt>;

// ============ EXPRESSIONS ============

struct LiteralExpr;
struct IdentifierExpr;
struct BinaryExpr;
struct UnaryExpr;
struct CallExpr;
struct IndexExpr;
struct ArrayExpr;
struct AssignExpr;
struct LogicalExpr;
struct LambdaExpr;
struct PropertyAccessExpr;
struct SelfExpr;
struct NewExpr;
struct SetExpr;
struct DictionaryExpr;

using ExprVariant = std::variant<
    std::shared_ptr<LiteralExpr>,
    std::shared_ptr<IdentifierExpr>,
    std::shared_ptr<BinaryExpr>,
    std::shared_ptr<UnaryExpr>,
    std::shared_ptr<CallExpr>,
    std::shared_ptr<IndexExpr>,
    std::shared_ptr<ArrayExpr>,
    std::shared_ptr<AssignExpr>,
    std::shared_ptr<LogicalExpr>,
    std::shared_ptr<LambdaExpr>,
    std::shared_ptr<PropertyAccessExpr>,
    std::shared_ptr<SelfExpr>,
    std::shared_ptr<NewExpr>,
    std::shared_ptr<SetExpr>,
    std::shared_ptr<DictionaryExpr>
>;

struct Expr {
    int line;
    ExprVariant variant;
    
    Expr(int line, ExprVariant var) : line(line), variant(std::move(var)) {}
};

// Literal expression (numbers, strings, booleans, nil)
struct LiteralExpr {
    std::variant<std::nullptr_t, double, std::string, bool> value;
    
    explicit LiteralExpr(std::nullptr_t) : value(nullptr) {}
    explicit LiteralExpr(double val) : value(val) {}
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
    
    CallExpr(ExprPtr callee, std::vector<ExprPtr> args)
        : callee(std::move(callee)), arguments(std::move(args)) {}
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
    
    explicit ArrayExpr(std::vector<ExprPtr> elems) : elements(std::move(elems)) {}
};

// Assignment expression
struct AssignExpr {
    std::string name; // Still kept for simple variable assignment optimizations
    ExprPtr value;
    ExprPtr index;    // For indexed assignment (arr[i] = val)
    ExprPtr object;   // For complex indexed assignment (obj.prop[i] = val)
    
    AssignExpr(const std::string& name, ExprPtr value, ExprPtr index = nullptr, ExprPtr object = nullptr)
        : name(name), value(std::move(value)), index(std::move(index)), object(std::move(object)) {}
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
    ExprPtr body;  // Expression body for single-expression lambdas
    std::vector<StmtPtr> stmtBody;  // Statement body for multi-statement lambdas
    
    LambdaExpr(std::vector<std::string> params, ExprPtr body)
        : params(std::move(params)), body(std::move(body)) {}
    
    LambdaExpr(std::vector<std::string> params, std::vector<StmtPtr> stmtBody)
        : params(std::move(params)), body(nullptr), stmtBody(std::move(stmtBody)) {}
};

// Property access expression (self.name or obj.property)
struct PropertyAccessExpr {
    ExprPtr object;
    std::string property;
    
    PropertyAccessExpr(ExprPtr obj, const std::string& prop)
        : object(std::move(obj)), property(prop) {}
};

// Self reference expression
struct SelfExpr {};

// New instance creation expression (model instantiation)
struct NewExpr {
    std::string className;
    std::vector<ExprPtr> arguments;
    
    NewExpr(const std::string& name, std::vector<ExprPtr> args)
        : className(name), arguments(std::move(args)) {}
};

// Property assignment expression (object.property = value)
struct SetExpr {
    ExprPtr object;
    std::string name;
    ExprPtr value;
    
    SetExpr(ExprPtr obj, const std::string& name, ExprPtr val)
        : object(std::move(obj)), name(name), value(std::move(val)) {}
};

// Dictionary literal expression { key: value, ... }
struct DictionaryExpr {
    std::vector<std::pair<ExprPtr, ExprPtr>> pairs;
    
    explicit DictionaryExpr(std::vector<std::pair<ExprPtr, ExprPtr>> pairs) 
        : pairs(std::move(pairs)) {}
};

// ============ STATEMENTS ============

struct ExprStmt;
struct OutStmt;
struct VarDeclStmt;
struct BlockStmt;
struct WhenStmt;
struct WhileStmt;
struct RepeatStmt;
struct GetStmt;
struct TaskStmt;
struct GiveStmt;
struct EscapeStmt;
struct SkipStmt;
struct ModelStmt;
struct StructStmt;
struct StructStmt;
struct UseStmt;
struct TryStmt;
struct ThrowStmt;

using StmtVariant = std::variant<
    std::shared_ptr<ExprStmt>,
    std::shared_ptr<OutStmt>,
    std::shared_ptr<VarDeclStmt>,
    std::shared_ptr<BlockStmt>,
    std::shared_ptr<WhenStmt>,
    std::shared_ptr<WhileStmt>,
    std::shared_ptr<RepeatStmt>,
    std::shared_ptr<GetStmt>,
    std::shared_ptr<TaskStmt>,
    std::shared_ptr<GiveStmt>,
    std::shared_ptr<EscapeStmt>,
    std::shared_ptr<SkipStmt>,
    std::shared_ptr<ModelStmt>,
    std::shared_ptr<StructStmt>,
    std::shared_ptr<UseStmt>,
    std::shared_ptr<TryStmt>,
    std::shared_ptr<ThrowStmt>
>;

struct Stmt {
    int line;
    StmtVariant variant;
    
    Stmt(int line, StmtVariant var) : line(line), variant(std::move(var)) {}
};

// Expression statement
struct ExprStmt {
    ExprPtr expression;
    
    explicit ExprStmt(ExprPtr expr) : expression(std::move(expr)) {}
};

// Output statement (out)
struct OutStmt {
    ExprPtr expression;
    
    explicit OutStmt(ExprPtr expr) : expression(std::move(expr)) {}
};

// Variable declaration
struct VarDeclStmt {
    std::string name;
    ExprPtr initializer;
    
    VarDeclStmt(const std::string& name, ExprPtr init)
        : name(name), initializer(std::move(init)) {}
};

// Block of statements
struct BlockStmt {
    std::vector<StmtPtr> statements;
    
    explicit BlockStmt(std::vector<StmtPtr> stmts) : statements(std::move(stmts)) {}
};

// If statement (when/other)
struct WhenStmt {
    ExprPtr condition;
    StmtPtr thenBranch;
    StmtPtr elseBranch; // May be nullptr
    
    WhenStmt(ExprPtr cond, StmtPtr thenBr, StmtPtr elseBr = nullptr)
        : condition(std::move(cond)), thenBranch(std::move(thenBr)), elseBranch(std::move(elseBr)) {}
};

// While loop
struct WhileStmt {
    ExprPtr condition;
    StmtPtr body;
    
    WhileStmt(ExprPtr cond, StmtPtr body)
        : condition(std::move(cond)), body(std::move(body)) {}
};

// Repeat loop (for loop)
struct RepeatStmt {
    std::string variable;
    ExprPtr start;
    ExprPtr end;
    StmtPtr body;
    
    RepeatStmt(const std::string& var, ExprPtr start, ExprPtr end, StmtPtr body)
        : variable(var), start(std::move(start)), end(std::move(end)), body(std::move(body)) {}
};

// Foreach loop (get x in array)
struct GetStmt {
    std::string variable;
    ExprPtr iterable;
    StmtPtr body;
    
    GetStmt(const std::string& var, ExprPtr iter, StmtPtr body)
        : variable(var), iterable(std::move(iter)), body(std::move(body)) {}
};

// Function definition (task)
struct TaskStmt {
    std::string name;
    std::vector<std::string> params;
    std::vector<StmtPtr> body;
    
    TaskStmt(const std::string& name, std::vector<std::string> params, std::vector<StmtPtr> body)
        : name(name), params(std::move(params)), body(std::move(body)) {}
};

// Return statement (give)
struct GiveStmt {
    ExprPtr value; // May be nullptr for bare 'give'
    
    explicit GiveStmt(ExprPtr val = nullptr) : value(std::move(val)) {}
};

// Break statement (escape)
struct EscapeStmt {};

// Continue statement (skip)
struct SkipStmt {};

// Member visibility
enum class MemberVisibility {
    PUBLIC,   // shown
    PRIVATE   // hidden
};

// Model member (property or method)
struct ModelMember {
    MemberVisibility visibility;
    bool isMethod;
    std::string name;
    ExprPtr initializer;  // For properties
    std::vector<std::string> params;  // For methods
    std::vector<StmtPtr> body;  // For methods
};

// Model (class) definition
struct ModelStmt {
    int line;
    std::string name;
    std::string parentName;  // For inheritance (empty if none)
    std::vector<std::string> initParams;
    std::vector<StmtPtr> initBody;
    std::vector<ModelMember> members;
    
    ModelStmt(int line, const std::string& name, const std::string& parent,
              std::vector<std::string> initParams, std::vector<StmtPtr> initBody,
              std::vector<ModelMember> members)
        : line(line), name(name), parentName(parent), initParams(std::move(initParams)),
          initBody(std::move(initBody)), members(std::move(members)) {}
};

// Struct definition (simplified class)
struct StructStmt {
    std::string name;
    std::vector<std::string> fields;
    
    StructStmt(const std::string& name, std::vector<std::string> fields)
        : name(name), fields(std::move(fields)) {}
};

// Use statement (import)
struct UseStmt {
    std::string path;
    
    explicit UseStmt(const std::string& path) : path(path) {}
};

// Try-Catch statement
struct TryStmt {
    StmtPtr tryBlock;
    std::string catchVar;
    StmtPtr catchBlock;
    
    TryStmt(StmtPtr tryBlk, const std::string& var, StmtPtr catchBlk)
        : tryBlock(std::move(tryBlk)), catchVar(var), catchBlock(std::move(catchBlk)) {}
};

// Throw statement (error)
struct ThrowStmt {
    ExprPtr expression;
    
    explicit ThrowStmt(ExprPtr expr) : expression(std::move(expr)) {}
};

// Helper functions to create expressions
inline ExprPtr makeLiteralExpr(int line, std::nullptr_t) {
    return std::make_shared<Expr>(line, std::make_shared<LiteralExpr>(nullptr));
}

inline ExprPtr makeLiteralExpr(int line, double val) {
    return std::make_shared<Expr>(line, std::make_shared<LiteralExpr>(val));
}

inline ExprPtr makeLiteralExpr(int line, const std::string& val) {
    return std::make_shared<Expr>(line, std::make_shared<LiteralExpr>(val));
}

inline ExprPtr makeLiteralExpr(int line, bool val) {
    return std::make_shared<Expr>(line, std::make_shared<LiteralExpr>(val));
}

inline ExprPtr makeIdentifierExpr(int line, const std::string& name) {
    return std::make_shared<Expr>(line, std::make_shared<IdentifierExpr>(name));
}

inline ExprPtr makeBinaryExpr(int line, ExprPtr left, TokenType op, ExprPtr right) {
    return std::make_shared<Expr>(line, std::make_shared<BinaryExpr>(std::move(left), op, std::move(right)));
}

inline ExprPtr makeUnaryExpr(int line, TokenType op, ExprPtr operand) {
    return std::make_shared<Expr>(line, std::make_shared<UnaryExpr>(op, std::move(operand)));
}

inline ExprPtr makeCallExpr(int line, ExprPtr callee, std::vector<ExprPtr> args) {
    return std::make_shared<Expr>(line, std::make_shared<CallExpr>(std::move(callee), std::move(args)));
}

inline ExprPtr makeIndexExpr(int line, ExprPtr object, ExprPtr index) {
    return std::make_shared<Expr>(line, std::make_shared<IndexExpr>(std::move(object), std::move(index)));
}

inline ExprPtr makeArrayExpr(int line, std::vector<ExprPtr> elements) {
    return std::make_shared<Expr>(line, std::make_shared<ArrayExpr>(std::move(elements)));
}

inline ExprPtr makeAssignExpr(int line, const std::string& name, ExprPtr value, ExprPtr index = nullptr, ExprPtr object = nullptr) {
    return std::make_shared<Expr>(line, std::make_shared<AssignExpr>(name, std::move(value), std::move(index), std::move(object)));
}

inline ExprPtr makeLogicalExpr(int line, ExprPtr left, TokenType op, ExprPtr right) {
    return std::make_shared<Expr>(line, std::make_shared<LogicalExpr>(std::move(left), op, std::move(right)));
}

inline ExprPtr makeLambdaExpr(int line, std::vector<std::string> params, ExprPtr body) {
    return std::make_shared<Expr>(line, std::make_shared<LambdaExpr>(std::move(params), std::move(body)));
}

inline ExprPtr makeLambdaExpr(int line, std::vector<std::string> params, std::vector<StmtPtr> stmtBody) {
    return std::make_shared<Expr>(line, std::make_shared<LambdaExpr>(std::move(params), std::move(stmtBody)));
}

// Helper functions to create statements
inline StmtPtr makeExprStmt(int line, ExprPtr expr) {
    return std::make_shared<Stmt>(line, std::make_shared<ExprStmt>(std::move(expr)));
}

inline StmtPtr makeOutStmt(int line, ExprPtr expr) {
    return std::make_shared<Stmt>(line, std::make_shared<OutStmt>(std::move(expr)));
}

inline StmtPtr makeVarDeclStmt(int line, const std::string& name, ExprPtr init) {
    return std::make_shared<Stmt>(line, std::make_shared<VarDeclStmt>(name, std::move(init)));
}

inline StmtPtr makeBlockStmt(int line, std::vector<StmtPtr> stmts) {
    return std::make_shared<Stmt>(line, std::make_shared<BlockStmt>(std::move(stmts)));
}

inline StmtPtr makeWhenStmt(int line, ExprPtr cond, StmtPtr thenBr, StmtPtr elseBr = nullptr) {
    return std::make_shared<Stmt>(line, std::make_shared<WhenStmt>(std::move(cond), std::move(thenBr), std::move(elseBr)));
}

inline StmtPtr makeWhileStmt(int line, ExprPtr cond, StmtPtr body) {
    return std::make_shared<Stmt>(line, std::make_shared<WhileStmt>(std::move(cond), std::move(body)));
}

inline StmtPtr makeRepeatStmt(int line, const std::string& var, ExprPtr start, ExprPtr end, StmtPtr body) {
    return std::make_shared<Stmt>(line, std::make_shared<RepeatStmt>(var, std::move(start), std::move(end), std::move(body)));
}

inline StmtPtr makeGetStmt(int line, const std::string& var, ExprPtr iter, StmtPtr body) {
    return std::make_shared<Stmt>(line, std::make_shared<GetStmt>(var, std::move(iter), std::move(body)));
}

inline StmtPtr makeTaskStmt(int line, const std::string& name, std::vector<std::string> params, std::vector<StmtPtr> body) {
    return std::make_shared<Stmt>(line, std::make_shared<TaskStmt>(name, std::move(params), std::move(body)));
}

inline StmtPtr makeGiveStmt(int line, ExprPtr val = nullptr) {
    return std::make_shared<Stmt>(line, std::make_shared<GiveStmt>(std::move(val)));
}

inline StmtPtr makeEscapeStmt(int line) {
    return std::make_shared<Stmt>(line, std::make_shared<EscapeStmt>());
}

inline StmtPtr makeSkipStmt(int line) {
    return std::make_shared<Stmt>(line, std::make_shared<SkipStmt>());
}

inline ExprPtr makePropertyAccessExpr(int line, ExprPtr object, const std::string& property) {
    return std::make_shared<Expr>(line, std::make_shared<PropertyAccessExpr>(std::move(object), property));
}

inline ExprPtr makeSelfExpr(int line) {
    return std::make_shared<Expr>(line, std::make_shared<SelfExpr>());
}

inline ExprPtr makeNewExpr(int line, const std::string& className, std::vector<ExprPtr> args) {
    return std::make_shared<Expr>(line, std::make_shared<NewExpr>(className, std::move(args)));
}

inline ExprPtr makeSetExpr(int line, ExprPtr object, const std::string& name, ExprPtr value) {
    return std::make_shared<Expr>(line, std::make_shared<SetExpr>(std::move(object), name, std::move(value)));
}

inline ExprPtr makeDictionaryExpr(int line, std::vector<std::pair<ExprPtr, ExprPtr>> pairs) {
    return std::make_shared<Expr>(line, std::make_shared<DictionaryExpr>(std::move(pairs)));
}

inline StmtPtr makeModelStmt(int line, const std::string& name, const std::string& parent,
                             std::vector<std::string> initParams, std::vector<StmtPtr> initBody,
                             std::vector<ModelMember> members) {
    return std::make_shared<Stmt>(line, std::make_shared<ModelStmt>(
        line, name, parent, std::move(initParams), std::move(initBody), std::move(members)));
}

inline StmtPtr makeStructStmt(int line, const std::string& name, std::vector<std::string> fields) {
    return std::make_shared<Stmt>(line, std::make_shared<StructStmt>(name, std::move(fields)));
}

inline StmtPtr makeUseStmt(int line, const std::string& path) {
    return std::make_shared<Stmt>(line, std::make_shared<UseStmt>(path));
}

inline StmtPtr makeTryStmt(int line, StmtPtr tryBlk, const std::string& var, StmtPtr catchBlk) {
    return std::make_shared<Stmt>(line, std::make_shared<TryStmt>(std::move(tryBlk), var, std::move(catchBlk)));
}

inline StmtPtr makeThrowStmt(int line, ExprPtr expr) {
    return std::make_shared<Stmt>(line, std::make_shared<ThrowStmt>(std::move(expr)));
}

#endif // AST_H
