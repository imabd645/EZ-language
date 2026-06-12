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
struct SpreadExpr;
struct TernaryExpr;
struct AwaitExpr;

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
    std::shared_ptr<DictionaryExpr>,
    std::shared_ptr<SpreadExpr>,
    std::shared_ptr<TernaryExpr>,
    std::shared_ptr<AwaitExpr>
>;

struct Expr {
    int line;
    std::string filename;
    ExprVariant variant;
    
    Expr(int line, const std::string& file, ExprVariant var) 
        : line(line), filename(file), variant(std::move(var)) {}
};

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
    bool isTailCall = false;
    
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
    bool isVariadic = false;
    bool isAsync = false;
    
    LambdaExpr(std::vector<std::string> params, ExprPtr body, bool variadic = false, bool isAsync = false)
        : params(std::move(params)), body(std::move(body)), isVariadic(variadic), isAsync(isAsync) {}
    
    LambdaExpr(std::vector<std::string> params, std::vector<StmtPtr> stmtBody, bool variadic = false, bool isAsync = false)
        : params(std::move(params)), body(nullptr), stmtBody(std::move(stmtBody)), isVariadic(variadic), isAsync(isAsync) {}
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

// ============ STATEMENTS ============

struct ExprStmt;
struct OutStmt;
struct VarDeclStmt;
struct BlockStmt;
struct WhenStmt;
struct WhileStmt;
struct RepeatStmt;
struct GetStmt;
struct MatchStmt;
struct TaskStmt;
struct GiveStmt;
struct EscapeStmt;
struct SkipStmt;
struct ModelStmt;
struct StructStmt;
struct InterfaceStmt;
struct UseStmt;
struct TryStmt;
struct ThrowStmt;
struct StaticStmt;

using StmtVariant = std::variant<
    std::shared_ptr<ExprStmt>,
    std::shared_ptr<OutStmt>,
    std::shared_ptr<VarDeclStmt>,
    std::shared_ptr<BlockStmt>,
    std::shared_ptr<WhenStmt>,
    std::shared_ptr<WhileStmt>,
    std::shared_ptr<RepeatStmt>,
    std::shared_ptr<GetStmt>,
    std::shared_ptr<MatchStmt>,
    std::shared_ptr<TaskStmt>,
    std::shared_ptr<GiveStmt>,
    std::shared_ptr<EscapeStmt>,
    std::shared_ptr<SkipStmt>,
    std::shared_ptr<ModelStmt>,
    std::shared_ptr<StructStmt>,
    std::shared_ptr<UseStmt>,
    std::shared_ptr<TryStmt>,
    std::shared_ptr<ThrowStmt>,
    std::shared_ptr<InterfaceStmt>,
    std::shared_ptr<StaticStmt>
>;

struct Stmt {
    int line;
    std::string filename;
    StmtVariant variant;
    
    Stmt(int line, const std::string& file, StmtVariant var) 
        : line(line), filename(file), variant(std::move(var)) {}
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

// Foreach loop (get x in array or get [k,v] in dict)
struct GetStmt {
    std::string variable;
    std::string valueVariable; // Optional second variable for dict KV iteration
    ExprPtr iterable;
    StmtPtr body;
    
    GetStmt(const std::string& var, const std::string& valVar, ExprPtr iter, StmtPtr body)
        : variable(var), valueVariable(valVar), iterable(std::move(iter)), body(std::move(body)) {}
};

struct MatchArm {
    ExprPtr pattern; // nullptr indicates the 'other' (default) arm
    StmtPtr body;
};

// Match statement (match x { 1 => ... other => ... })
struct MatchStmt {
    ExprPtr subject;
    std::vector<MatchArm> arms;
    
    MatchStmt(ExprPtr subject, std::vector<MatchArm> arms)
        : subject(std::move(subject)), arms(std::move(arms)) {}
};


// Function definition (task)
struct TaskStmt {
    std::string name;
    std::vector<std::string> params;
    std::vector<ExprPtr> defaultValues; // nullptr = required, ExprPtr = default value
    std::vector<StmtPtr> body;
    bool isVariadic = false;
    bool isAsync = false;
    
    TaskStmt(const std::string& name, std::vector<std::string> params, 
             std::vector<ExprPtr> defaultValues, std::vector<StmtPtr> body, bool variadic = false, bool isAsync = false)
        : name(name), params(std::move(params)), defaultValues(std::move(defaultValues)),
          body(std::move(body)), isVariadic(variadic), isAsync(isAsync) {}
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
    bool isStatic = false;
    bool isMethod;
    bool isAsync = false;
    std::string name;
    ExprPtr initializer;  // For properties
    std::vector<std::string> params;  // For methods
    std::vector<ExprPtr> defaultValues; // For methods
    std::vector<StmtPtr> body;  // For methods
};

// Static variable declaration (persistent across task calls)
struct StaticStmt {
    std::string name;
    ExprPtr initializer;
    
    StaticStmt(const std::string& name, ExprPtr init)
        : name(name), initializer(std::move(init)) {}
};

// Interface definition
struct InterfaceStmt {
    int line;
    std::string name;
    std::vector<std::string> methods;
    
    InterfaceStmt(int line, const std::string& name, std::vector<std::string> methods)
        : line(line), name(name), methods(std::move(methods)) {}
};

// Model (class) definition
struct ModelStmt {
    int line;
    std::string name;
    std::string parentName;  // For inheritance (empty if none)
    std::vector<std::string> interfaces; // For implements
    std::vector<std::string> initParams;
    std::vector<ExprPtr> initDefaultValues;
    std::vector<StmtPtr> initBody;
    std::vector<ModelMember> members;
    
    ModelStmt(int line, const std::string& name, const std::string& parent,
              std::vector<std::string> interfaces,
              std::vector<std::string> initParams, 
              std::vector<ExprPtr> initDefaultValues,
              std::vector<StmtPtr> initBody,
              std::vector<ModelMember> members)
        : line(line), name(name), parentName(parent), interfaces(std::move(interfaces)),
          initParams(std::move(initParams)), initDefaultValues(std::move(initDefaultValues)),
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
    std::string alias;
    
    UseStmt(const std::string& path, const std::string& alias = "") 
        : path(path), alias(alias) {}
};

struct CatchBlock {
    std::string typeName; // Optional (e.g., catch MyError e)
    std::string varName;
    StmtPtr body;
};

// Try-Catch statement
struct TryStmt {
    StmtPtr tryBlock;
    std::vector<CatchBlock> catchBlocks;
    
    TryStmt(StmtPtr tryBlk, std::vector<CatchBlock> catchBlocks)
        : tryBlock(std::move(tryBlk)), catchBlocks(std::move(catchBlocks)) {}
};

// Throw statement (error)
struct ThrowStmt {
    ExprPtr expression;
    
    explicit ThrowStmt(ExprPtr expr) : expression(std::move(expr)) {}
};

// Helper functions to create expressions
inline ExprPtr makeLiteralExpr(int line, const std::string& file, std::nullptr_t) {
    return std::make_shared<Expr>(line, file, std::make_shared<LiteralExpr>(nullptr));
}

inline ExprPtr makeLiteralExpr(int line, const std::string& file, double val) {
    return std::make_shared<Expr>(line, file, std::make_shared<LiteralExpr>(val));
}

inline ExprPtr makeLiteralExpr(int line, const std::string& file, long long val) {
    return std::make_shared<Expr>(line, file, std::make_shared<LiteralExpr>(val));
}

inline ExprPtr makeLiteralExpr(int line, const std::string& file, const std::string& val) {
    return std::make_shared<Expr>(line, file, std::make_shared<LiteralExpr>(val));
}

inline ExprPtr makeLiteralExpr(int line, const std::string& file, bool val) {
    return std::make_shared<Expr>(line, file, std::make_shared<LiteralExpr>(val));
}

inline ExprPtr makeIdentifierExpr(int line, const std::string& file, const std::string& name) {
    return std::make_shared<Expr>(line, file, std::make_shared<IdentifierExpr>(name));
}

inline ExprPtr makeBinaryExpr(int line, const std::string& file, ExprPtr left, TokenType op, ExprPtr right) {
    return std::make_shared<Expr>(line, file, std::make_shared<BinaryExpr>(std::move(left), op, std::move(right)));
}

inline ExprPtr makeUnaryExpr(int line, const std::string& file, TokenType op, ExprPtr operand) {
    return std::make_shared<Expr>(line, file, std::make_shared<UnaryExpr>(op, std::move(operand)));
}

inline ExprPtr makeCallExpr(int line, const std::string& file, ExprPtr callee, std::vector<ExprPtr> args) {
    return std::make_shared<Expr>(line, file, std::make_shared<CallExpr>(std::move(callee), std::move(args)));
}

inline ExprPtr makeIndexExpr(int line, const std::string& file, ExprPtr object, ExprPtr index) {
    return std::make_shared<Expr>(line, file, std::make_shared<IndexExpr>(std::move(object), std::move(index)));
}

inline ExprPtr makeArrayExpr(int line, const std::string& file, std::vector<ExprPtr> elements) {
    return std::make_shared<Expr>(line, file, std::make_shared<ArrayExpr>(std::move(elements)));
}

inline ExprPtr makeAssignExpr(int line, const std::string& file, const std::string& name, ExprPtr value, ExprPtr index = nullptr, ExprPtr object = nullptr) {
    return std::make_shared<Expr>(line, file, std::make_shared<AssignExpr>(name, std::move(value), std::move(index), std::move(object)));
}

inline ExprPtr makeLogicalExpr(int line, const std::string& file, ExprPtr left, TokenType op, ExprPtr right) {
    return std::make_shared<Expr>(line, file, std::make_shared<LogicalExpr>(std::move(left), op, std::move(right)));
}

inline ExprPtr makeLambdaExpr(int line, const std::string& file, std::vector<std::string> params, ExprPtr body, bool variadic = false, bool isAsync = false) {
    return std::make_shared<Expr>(line, file, std::make_shared<LambdaExpr>(std::move(params), std::move(body), variadic, isAsync));
}

inline ExprPtr makeLambdaExpr(int line, const std::string& file, std::vector<std::string> params, std::vector<StmtPtr> stmtBody, bool variadic = false, bool isAsync = false) {
    return std::make_shared<Expr>(line, file, std::make_shared<LambdaExpr>(std::move(params), std::move(stmtBody), variadic, isAsync));
}

// Helper functions to create statements
inline StmtPtr makeExprStmt(int line, const std::string& file, ExprPtr expr) {
    return std::make_shared<Stmt>(line, file, std::make_shared<ExprStmt>(std::move(expr)));
}

inline StmtPtr makeOutStmt(int line, const std::string& file, ExprPtr expr) {
    return std::make_shared<Stmt>(line, file, std::make_shared<OutStmt>(std::move(expr)));
}

inline StmtPtr makeVarDeclStmt(int line, const std::string& file, const std::string& name, ExprPtr init) {
    return std::make_shared<Stmt>(line, file, std::make_shared<VarDeclStmt>(name, std::move(init)));
}

inline StmtPtr makeBlockStmt(int line, const std::string& file, std::vector<StmtPtr> stmts) {
    return std::make_shared<Stmt>(line, file, std::make_shared<BlockStmt>(std::move(stmts)));
}

inline StmtPtr makeWhenStmt(int line, const std::string& file, ExprPtr cond, StmtPtr thenBr, StmtPtr elseBr = nullptr) {
    return std::make_shared<Stmt>(line, file, std::make_shared<WhenStmt>(std::move(cond), std::move(thenBr), std::move(elseBr)));
}

inline StmtPtr makeWhileStmt(int line, const std::string& file, ExprPtr cond, StmtPtr body) {
    return std::make_shared<Stmt>(line, file, std::make_shared<WhileStmt>(std::move(cond), std::move(body)));
}

inline StmtPtr makeRepeatStmt(int line, const std::string& file, const std::string& var, ExprPtr start, ExprPtr end, StmtPtr body) {
    return std::make_shared<Stmt>(line, file, std::make_shared<RepeatStmt>(var, std::move(start), std::move(end), std::move(body)));
}

inline StmtPtr makeGetStmt(int line, const std::string& file, const std::string& var, ExprPtr iter, StmtPtr body) {
    return std::make_shared<Stmt>(line, file, std::make_shared<GetStmt>(var, "", std::move(iter), std::move(body)));
}

inline StmtPtr makeGetKVStmt(int line, const std::string& file, const std::string& keyVar, const std::string& valVar, ExprPtr iter, StmtPtr body) {
    return std::make_shared<Stmt>(line, file, std::make_shared<GetStmt>(keyVar, valVar, std::move(iter), std::move(body)));
}

inline StmtPtr makeMatchStmt(int line, const std::string& file, ExprPtr subject, std::vector<MatchArm> arms) {
    return std::make_shared<Stmt>(line, file, std::make_shared<MatchStmt>(std::move(subject), std::move(arms)));
}

inline StmtPtr makeTaskStmt(int line, const std::string& file, const std::string& name, std::vector<std::string> params, 
                            std::vector<ExprPtr> defaultValues, std::vector<StmtPtr> body, bool variadic = false, bool isAsync = false) {
    return std::make_shared<Stmt>(line, file, std::make_shared<TaskStmt>(name, std::move(params), 
                                                                     std::move(defaultValues), std::move(body), variadic, isAsync));
}

inline StmtPtr makeGiveStmt(int line, const std::string& file, ExprPtr val = nullptr) {
    return std::make_shared<Stmt>(line, file, std::make_shared<GiveStmt>(std::move(val)));
}

inline StmtPtr makeEscapeStmt(int line, const std::string& file) {
    return std::make_shared<Stmt>(line, file, std::make_shared<EscapeStmt>());
}

inline StmtPtr makeSkipStmt(int line, const std::string& file) {
    return std::make_shared<Stmt>(line, file, std::make_shared<SkipStmt>());
}

inline ExprPtr makePropertyAccessExpr(int line, const std::string& file, ExprPtr object, const std::string& property) {
    return std::make_shared<Expr>(line, file, std::make_shared<PropertyAccessExpr>(std::move(object), property));
}

inline ExprPtr makeSelfExpr(int line, const std::string& file) {
    return std::make_shared<Expr>(line, file, std::make_shared<SelfExpr>());
}

inline ExprPtr makeNewExpr(int line, const std::string& file, const std::string& className, std::vector<ExprPtr> args) {
    return std::make_shared<Expr>(line, file, std::make_shared<NewExpr>(className, std::move(args)));
}

inline ExprPtr makeSetExpr(int line, const std::string& file, ExprPtr object, const std::string& name, ExprPtr value) {
    return std::make_shared<Expr>(line, file, std::make_shared<SetExpr>(std::move(object), name, std::move(value)));
}

inline ExprPtr makeDictionaryExpr(int line, const std::string& file, std::vector<std::pair<ExprPtr, ExprPtr>> pairs) {
    return std::make_shared<Expr>(line, file, std::make_shared<DictionaryExpr>(std::move(pairs)));
}

inline ExprPtr makeSpreadExpr(int line, const std::string& file, ExprPtr expr) {
    return std::make_shared<Expr>(line, file, std::make_shared<SpreadExpr>(std::move(expr)));
}

inline ExprPtr makeTernaryExpr(int line, const std::string& file, ExprPtr cond, ExprPtr thenBr, ExprPtr elseBr) {
    return std::make_shared<Expr>(line, file, std::make_shared<TernaryExpr>(std::move(cond), std::move(thenBr), std::move(elseBr)));
}

inline StmtPtr makeInterfaceStmt(int line, const std::string& file, const std::string& name, std::vector<std::string> methods) {
    return std::make_shared<Stmt>(line, file, std::make_shared<InterfaceStmt>(line, name, std::move(methods)));
}

inline StmtPtr makeModelStmt(int line, const std::string& file, const std::string& name, const std::string& parent,
                             std::vector<std::string> interfaces,
                             std::vector<std::string> initParams, 
                             std::vector<ExprPtr> initDefaultValues,
                             std::vector<StmtPtr> initBody,
                             std::vector<ModelMember> members) {
    return std::make_shared<Stmt>(line, file, std::make_shared<ModelStmt>(
        line, name, parent, std::move(interfaces), std::move(initParams), 
        std::move(initDefaultValues), std::move(initBody), std::move(members)));
}

inline StmtPtr makeStructStmt(int line, const std::string& file, const std::string& name, std::vector<std::string> fields) {
    return std::make_shared<Stmt>(line, file, std::make_shared<StructStmt>(name, std::move(fields)));
}

inline StmtPtr makeUseStmt(int line, const std::string& file, const std::string& path, const std::string& alias = "") {
    return std::make_shared<Stmt>(line, file, std::make_shared<UseStmt>(path, alias));
}

inline StmtPtr makeTryStmt(int line, const std::string& file, StmtPtr tryBlk, std::vector<CatchBlock> catchBlocks) {
    return std::make_shared<Stmt>(line, file, std::make_shared<TryStmt>(std::move(tryBlk), std::move(catchBlocks)));
}

inline StmtPtr makeThrowStmt(int line, const std::string& file, ExprPtr expr) {
    return std::make_shared<Stmt>(line, file, std::make_shared<ThrowStmt>(std::move(expr)));
}

inline StmtPtr makeStaticStmt(int line, const std::string& file, const std::string& name, ExprPtr init) {
    return std::make_shared<Stmt>(line, file, std::make_shared<StaticStmt>(name, std::move(init)));
}

#endif // AST_H
