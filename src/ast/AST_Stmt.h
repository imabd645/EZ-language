#ifndef AST_STMT_H
#define AST_STMT_H

#include "AST_Core.h"
#include "AST_Expr.h"

// Block of statements

struct ExpressionStmt {
    ExprPtr expr;
    explicit ExpressionStmt(ExprPtr expr) : expr(std::move(expr)) {}
};

struct OutStmt {
    ExprPtr expr;
    explicit OutStmt(ExprPtr expr) : expr(std::move(expr)) {}
};

struct VarDeclStmt {
    std::string name;
    ExprPtr initializer;
    TypeASTPtr typeHint;
    VarDeclStmt(std::string name, ExprPtr initializer, TypeASTPtr typeHint = nullptr)
        : name(std::move(name)), initializer(std::move(initializer)), typeHint(std::move(typeHint)) {}
};

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
    ExprPtr step;
    StmtPtr body;
    
    RepeatStmt(const std::string& var, ExprPtr s, ExprPtr e, ExprPtr st, StmtPtr b)
        : variable(var), start(std::move(s)), end(std::move(e)), step(std::move(st)), body(std::move(b)) {}
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
// Rate-limit configuration for @ratelimit decorator
struct RateLimitConfig {
    int         count;    // max calls per window
    std::string per;      // "second","minute","hour","day"
    ExprPtr     keyExpr;  // nullptr = "global"
};

struct TaskStmt {
    std::string name;
    std::vector<std::string> params;
    std::vector<TypeASTPtr> paramTypes;
    std::vector<ExprPtr> defaultValues; // nullptr = required, ExprPtr = default value
    TypeASTPtr returnType;
    std::vector<StmtPtr> body;
    bool isVariadic = false;
    bool isAsync = false;
    // Design-by-Contract clauses
    std::vector<std::pair<ExprPtr, std::string>> requiresClauses; // {condition, message}
    std::vector<std::pair<ExprPtr, std::string>> ensuresClauses;  // {condition, message}
    // old() captures: {hidden_var_name, expr_to_capture}
    std::vector<std::pair<std::string, ExprPtr>> oldCaptures;
    // Decorator flags
    bool isCached = false;
    std::shared_ptr<RateLimitConfig> rateLimit = nullptr; // nullptr = not rate limited
    std::vector<std::string> userDecorators; // Custom user-defined decorators
    std::vector<std::string> typeParams;
    
    TaskStmt(const std::string& name, std::vector<std::string> params, std::vector<TypeASTPtr> paramTypes,
             std::vector<ExprPtr> defaultValues, TypeASTPtr returnType, std::vector<StmtPtr> body, bool variadic = false, bool isAsync = false)
        : name(name), params(std::move(params)), paramTypes(std::move(paramTypes)), defaultValues(std::move(defaultValues)),
          returnType(std::move(returnType)), body(std::move(body)), isVariadic(variadic), isAsync(isAsync) {}
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

// Validate rule for @validate decorator on model fields
struct ValidateRule {
    std::string ruleName;  // "minlen","maxlen","min","max","email","pattern","notnull"
    ExprPtr     param;     // nullptr for rules without params
    std::string message;   // custom message or auto-generated
};

// Model member (property or method)
struct ModelMember {
    MemberVisibility visibility;
    bool isStatic = false;
    bool isMethod;
    bool isAsync = false;
    bool isCached = false;   // @cached decorator on methods
    std::string name;
    std::vector<std::string> typeParams; // For generic methods
    TypeASTPtr typeHint; // For properties and methods (return type)
    ExprPtr initializer;  // For properties
    std::vector<std::string> params;  // For methods
    std::vector<TypeASTPtr> paramTypes; // For methods
    std::vector<ExprPtr> defaultValues; // For methods
    std::vector<StmtPtr> body;  // For methods
    std::vector<ValidateRule> validators; // @validate rules (for properties)
};

// Static variable declaration (persistent across task calls)
struct StaticStmt {
    std::string name;
    ExprPtr initializer;
    
    StaticStmt(const std::string& name, ExprPtr init)
        : name(name), initializer(std::move(init)) {}
};

struct InterfaceMethod {
    std::string name;
    std::vector<std::string> params;
    std::vector<TypeASTPtr> paramTypes;
    TypeASTPtr returnType;
};

// Interface definition
struct InterfaceStmt {
    int line;
    std::string name;
    std::vector<InterfaceMethod> methods;
    std::vector<std::string> typeParams;
    
    InterfaceStmt(int line, const std::string& name, std::vector<InterfaceMethod> methods)
        : line(line), name(name), methods(std::move(methods)) {}
};

// Model (class) definition
struct ModelStmt {
    int line;
    std::string name;
    std::string parentName;  // For inheritance (empty if none)
    std::vector<std::string> interfaces; // For implements
    std::vector<std::string> initParams;
    std::vector<TypeASTPtr> initParamTypes;
    std::vector<ExprPtr> initDefaultValues;
    std::vector<StmtPtr> initBody;
    std::vector<ModelMember> members;
    // Decorator fields
    bool audited  = false;     // @audited
    bool snapshot = false;     // @snapshot
    std::string persistPath;   // @persist("file")  empty = not persistent
    std::vector<std::string> userDecorators; // Custom user-defined decorators
    std::vector<std::string> typeParams;
        
    ModelStmt(int line, const std::string& name, const std::string& parent,
              std::vector<std::string> interfaces,
              std::vector<std::string> initParams, 
              std::vector<TypeASTPtr> initParamTypes,
              std::vector<ExprPtr> initDefaultValues,
              std::vector<StmtPtr> initBody,
              std::vector<ModelMember> members)
        : line(line), name(name), parentName(parent), interfaces(std::move(interfaces)),
          initParams(std::move(initParams)), initParamTypes(std::move(initParamTypes)), initDefaultValues(std::move(initDefaultValues)),
          initBody(std::move(initBody)), members(std::move(members)) {}
};

// Struct definition (simplified class)
struct StructStmt {
    std::string name;
    std::vector<std::string> fields;
    std::vector<TypeASTPtr> types;
    std::vector<ExprPtr> defaults;
    std::vector<std::string> typeParams;
    
    StructStmt(const std::string& name, std::vector<std::string> fields, std::vector<TypeASTPtr> types, std::vector<ExprPtr> defaults)
        : name(name), fields(std::move(fields)), types(std::move(types)), defaults(std::move(defaults)) {}
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
    ExprPtr expr;
    
    explicit ThrowStmt(ExprPtr expr) : expr(std::move(expr)) {}
};

// Export statement  marks a declaration as publicly visible in namespaced module imports
struct ExportStmt {
    StmtPtr inner;  // The wrapped task/variable/model declaration
    explicit ExportStmt(StmtPtr inner) : inner(std::move(inner)) {}
};

// Helper functions to create expressions
inline ExprPtr makeLiteralExpr(int line, int column, int length, const std::string& file, std::nullptr_t) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<LiteralExpr>(nullptr));
}

inline ExprPtr makeLiteralExpr(int line, int column, int length, const std::string& file, double val) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<LiteralExpr>(val));
}

inline ExprPtr makeLiteralExpr(int line, int column, int length, const std::string& file, long long val) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<LiteralExpr>(val));
}

inline ExprPtr makeLiteralExpr(int line, int column, int length, const std::string& file, const std::string& val) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<LiteralExpr>(val));
}

inline ExprPtr makeLiteralExpr(int line, int column, int length, const std::string& file, bool val) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<LiteralExpr>(val));
}

inline ExprPtr makeIdentifierExpr(int line, int column, int length, const std::string& file, const std::string& name) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<IdentifierExpr>(name));
}

inline ExprPtr makeBinaryExpr(int line, int column, int length, const std::string& file, ExprPtr left, TokenType op, ExprPtr right) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<BinaryExpr>(std::move(left), op, std::move(right)));
}

inline ExprPtr makeUnaryExpr(int line, int column, int length, const std::string& file, TokenType op, ExprPtr operand) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<UnaryExpr>(op, std::move(operand)));
}

inline ExprPtr makeCallExpr(int line, int column, int length, const std::string& file, ExprPtr callee, std::vector<ExprPtr> args, std::vector<std::string> argNames = {}) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<CallExpr>(std::move(callee), std::move(args), std::move(argNames)));
}

inline ExprPtr makeIndexExpr(int line, int column, int length, const std::string& file, ExprPtr object, ExprPtr index) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<IndexExpr>(std::move(object), std::move(index)));
}

inline ExprPtr makeArrayExpr(int line, int column, int length, const std::string& file, std::vector<ExprPtr> elements) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<ArrayExpr>(std::move(elements)));
}

inline ExprPtr makeTupleExpr(int line, int column, int length, const std::string& file, std::vector<ExprPtr> elements) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<TupleExpr>(std::move(elements)));
}

inline ExprPtr makeAssignExpr(int line, int column, int length, const std::string& file, const std::string& name, ExprPtr value, ExprPtr index = nullptr, ExprPtr object = nullptr) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<AssignExpr>(name, std::move(value), std::move(index), std::move(object)));
}

inline ExprPtr makeDestructureAssignExpr(int line, int column, int length, const std::string& file, std::vector<ExprPtr> targets, ExprPtr value) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<DestructureAssignExpr>(std::move(targets), std::move(value)));
}

inline ExprPtr makeLogicalExpr(int line, int column, int length, const std::string& file, ExprPtr left, TokenType op, ExprPtr right) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<LogicalExpr>(std::move(left), op, std::move(right)));
}

inline ExprPtr makeLambdaExpr(int line, int column, int length, const std::string& file, std::vector<std::string> params, std::vector<TypeASTPtr> paramTypes, ExprPtr body, TypeASTPtr returnType = nullptr, bool variadic = false, bool isAsync = false) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<LambdaExpr>(std::move(params), std::move(paramTypes), std::move(body), std::move(returnType), variadic, isAsync));
}

inline ExprPtr makeLambdaExpr(int line, int column, int length, const std::string& file, std::vector<std::string> params, std::vector<TypeASTPtr> paramTypes, std::vector<StmtPtr> stmtBody, TypeASTPtr returnType = nullptr, bool variadic = false, bool isAsync = false) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<LambdaExpr>(std::move(params), std::move(paramTypes), std::move(stmtBody), std::move(returnType), variadic, isAsync));
}

// Helper functions to create statements
inline StmtPtr makeExpressionStmt(int line, int column, int length, const std::string& file, ExprPtr expr) {
    return std::make_shared<Stmt>(line, column, length, file, std::make_shared<ExpressionStmt>(std::move(expr)));
}

inline StmtPtr makeOutStmt(int line, int column, int length, const std::string& file, ExprPtr expr) {
    return std::make_shared<Stmt>(line, column, length, file, std::make_shared<OutStmt>(std::move(expr)));
}

inline StmtPtr makeVarDeclStmt(int line, int column, int length, const std::string& file, const std::string& name, ExprPtr init, TypeASTPtr typeHint = nullptr) {
    return std::make_shared<Stmt>(line, column, length, file, std::make_shared<VarDeclStmt>(name, std::move(init), std::move(typeHint)));
}

inline StmtPtr makeBlockStmt(int line, int column, int length, const std::string& file, std::vector<StmtPtr> stmts) {
    return std::make_shared<Stmt>(line, column, length, file, std::make_shared<BlockStmt>(std::move(stmts)));
}

inline StmtPtr makeWhenStmt(int line, int column, int length, const std::string& file, ExprPtr cond, StmtPtr thenBr, StmtPtr elseBr = nullptr) {
    return std::make_shared<Stmt>(line, column, length, file, std::make_shared<WhenStmt>(std::move(cond), std::move(thenBr), std::move(elseBr)));
}

inline StmtPtr makeWhileStmt(int line, int column, int length, const std::string& file, ExprPtr cond, StmtPtr body) {
    return std::make_shared<Stmt>(line, column, length, file, std::make_shared<WhileStmt>(std::move(cond), std::move(body)));
}

inline StmtPtr makeRepeatStmt(int line, int column, int length, const std::string& file, const std::string& var, ExprPtr start, ExprPtr end, ExprPtr step, StmtPtr body) {
    return std::make_shared<Stmt>(line, column, length, file, std::make_shared<RepeatStmt>(var, std::move(start), std::move(end), std::move(step), std::move(body)));
}

inline StmtPtr makeGetStmt(int line, int column, int length, const std::string& file, const std::string& var, ExprPtr iter, StmtPtr body) {
    return std::make_shared<Stmt>(line, column, length, file, std::make_shared<GetStmt>(var, "", std::move(iter), std::move(body)));
}

inline StmtPtr makeGetKVStmt(int line, int column, int length, const std::string& file, const std::string& keyVar, const std::string& valVar, ExprPtr iter, StmtPtr body) {
    return std::make_shared<Stmt>(line, column, length, file, std::make_shared<GetStmt>(keyVar, valVar, std::move(iter), std::move(body)));
}

inline StmtPtr makeMatchStmt(int line, int column, int length, const std::string& file, ExprPtr subject, std::vector<MatchArm> arms) {
    return std::make_shared<Stmt>(line, column, length, file, std::make_shared<MatchStmt>(std::move(subject), std::move(arms)));
}

inline StmtPtr makeTaskStmt(int line, int column, int length, const std::string& file, const std::string& name, std::vector<std::string> params, std::vector<TypeASTPtr> paramTypes,
                            std::vector<ExprPtr> defaultValues, TypeASTPtr returnType, std::vector<StmtPtr> body, bool variadic = false, bool isAsync = false) {
    return std::make_shared<Stmt>(line, column, length, file, std::make_shared<TaskStmt>(name, std::move(params), std::move(paramTypes),
                                                                     std::move(defaultValues), std::move(returnType), std::move(body), variadic, isAsync));
}

inline StmtPtr makeGiveStmt(int line, int column, int length, const std::string& file, ExprPtr val = nullptr) {
    return std::make_shared<Stmt>(line, column, length, file, std::make_shared<GiveStmt>(std::move(val)));
}

inline StmtPtr makeEscapeStmt(int line, int column, int length, const std::string& file) {
    return std::make_shared<Stmt>(line, column, length, file, std::make_shared<EscapeStmt>());
}

inline StmtPtr makeSkipStmt(int line, int column, int length, const std::string& file) {
    return std::make_shared<Stmt>(line, column, length, file, std::make_shared<SkipStmt>());
}

inline ExprPtr makePropertyAccessExpr(int line, int column, int length, const std::string& file, ExprPtr obj, const std::string& prop, bool isOptional = false) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<PropertyAccessExpr>(std::move(obj), prop, isOptional));
}

inline ExprPtr makeSelfExpr(int line, int column, int length, const std::string& file) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<SelfExpr>());
}

inline ExprPtr makeSuperExpr(int line, int column, int length, const std::string& file) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<SuperExpr>());
}

inline ExprPtr makeNewExpr(int line, int column, int length, const std::string& file, const std::string& className, std::vector<ExprPtr> args, std::vector<TypeASTPtr> typeArgs = {}) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<NewExpr>(className, std::move(args), std::move(typeArgs)));
}

inline ExprPtr makeSetExpr(int line, int column, int length, const std::string& file, ExprPtr object, const std::string& name, ExprPtr value) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<SetExpr>(std::move(object), name, std::move(value)));
}

inline ExprPtr makeDictionaryExpr(int line, int column, int length, const std::string& file, std::vector<std::pair<ExprPtr, ExprPtr>> pairs) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<DictionaryExpr>(std::move(pairs)));
}

inline ExprPtr makeSpreadExpr(int line, int column, int length, const std::string& file, ExprPtr expr) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<SpreadExpr>(std::move(expr)));
}

inline ExprPtr makeTernaryExpr(int line, int column, int length, const std::string& file, ExprPtr cond, ExprPtr thenBr, ExprPtr elseBr) {
    return std::make_shared<Expr>(line, column, length, file, std::make_shared<TernaryExpr>(std::move(cond), std::move(thenBr), std::move(elseBr)));
}

inline StmtPtr makeInterfaceStmt(int line, int column, int length, const std::string& file, const std::string& name, std::vector<InterfaceMethod> methods) {
    return std::make_shared<Stmt>(line, column, length, file, std::make_shared<InterfaceStmt>(line, name, std::move(methods)));
}

inline StmtPtr makeModelStmt(int line, int column, int length, const std::string& file, const std::string& name, const std::string& parent,
                             std::vector<std::string> interfaces,
                             std::vector<std::string> initParams, 
                             std::vector<TypeASTPtr> initParamTypes,
                             std::vector<ExprPtr> initDefaultValues,
                             std::vector<StmtPtr> initBody,
                             std::vector<ModelMember> members) {
    return std::make_shared<Stmt>(line, column, length, file, std::make_shared<ModelStmt>(
        line, name, parent, std::move(interfaces), std::move(initParams), std::move(initParamTypes),
        std::move(initDefaultValues), std::move(initBody), std::move(members)));
}

inline StmtPtr makeStructStmt(int line, int column, int length, const std::string& file, const std::string& name, std::vector<std::string> fields, std::vector<TypeASTPtr> types, std::vector<ExprPtr> defaults) {
    return std::make_shared<Stmt>(line, column, length, file, std::make_shared<StructStmt>(name, std::move(fields), std::move(types), std::move(defaults)));
}

inline StmtPtr makeUseStmt(int line, int column, int length, const std::string& file, const std::string& path, const std::string& alias = "") {
    return std::make_shared<Stmt>(line, column, length, file, std::make_shared<UseStmt>(path, alias));
}

inline StmtPtr makeTryStmt(int line, int column, int length, const std::string& file, StmtPtr tryBlk, std::vector<CatchBlock> catchBlocks) {
    return std::make_shared<Stmt>(line, column, length, file, std::make_shared<TryStmt>(std::move(tryBlk), std::move(catchBlocks)));
}

inline StmtPtr makeThrowStmt(int line, int column, int length, const std::string& file, ExprPtr expr) {
    return std::make_shared<Stmt>(line, column, length, file, std::make_shared<ThrowStmt>(std::move(expr)));
}

inline StmtPtr makeStaticStmt(int line, int column, int length, const std::string& file, const std::string& name, ExprPtr init) {
    return std::make_shared<Stmt>(line, column, length, file, std::make_shared<StaticStmt>(name, std::move(init)));
}

inline StmtPtr makeExportStmt(int line, int column, int length, const std::string& file, StmtPtr inner) {
    return std::make_shared<Stmt>(line, column, length, file, std::make_shared<ExportStmt>(std::move(inner)));
}


#endif // AST_STMT_H
