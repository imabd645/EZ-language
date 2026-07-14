#ifndef AST_CORE_H
#define AST_CORE_H

#include <memory>
#include <vector>
#include <string>
#include <variant>
#include <optional>
#include "lexer/Token.h"

// Forward declarations
struct Expr;
struct Stmt;

using ExprPtr = Expr*;
using StmtPtr = Stmt*;

struct TypeAST;
using TypeASTPtr = TypeAST*;

struct TypeAST {
    std::string baseType;
    std::vector<TypeASTPtr> typeArgs;
    
    TypeAST(const std::string& base) : baseType(base) {}
    TypeAST(const std::string& base, std::vector<TypeASTPtr> args) : baseType(base), typeArgs(std::move(args)) {}
};



// Forward Declarations for Expressions
struct LiteralExpr;
struct IdentifierExpr;
struct BinaryExpr;
struct UnaryExpr;
struct CallExpr;
struct IndexExpr;
struct ArrayExpr;
struct TupleExpr;
struct AssignExpr;
struct LogicalExpr;
struct LambdaExpr;
struct PropertyAccessExpr;
struct SelfExpr;
struct SuperExpr;
struct NewExpr;
struct SetExpr;
struct DictionaryExpr;
struct SpreadExpr;
struct TernaryExpr;
struct AwaitExpr;
struct DestructureAssignExpr;

// Forward Declarations for Statements
struct BlockStmt;
struct ExpressionStmt;
struct VarDeclStmt;
struct OutStmt;
struct IfStmt;
struct WhileStmt;
struct RepeatStmt;
struct BreakStmt;
struct ContinueStmt;
struct FunctionDeclStmt;
struct ReturnStmt;
struct ClassDeclStmt;
struct InterfaceDeclStmt;
struct ArrayAssignStmt;
struct ImportStmt;
struct ExportStmt;
struct PropertyAssignStmt;
struct TryStmt;
struct ThrowStmt;
struct GetStmt;
struct TupleAssignStmt;
struct StructDeclStmt;
struct ModelDeclStmt;
struct MatchStmt;
struct TaskStmt;
struct GiveStmt;
struct EscapeStmt;
struct SkipStmt;
struct StaticStmt;
struct WhenStmt;
struct InterfaceStmt;
struct ModelStmt;
struct StructStmt;
struct UseStmt;

using ExprVariant = std::variant<
    LiteralExpr*,
    IdentifierExpr*,
    BinaryExpr*,
    UnaryExpr*,
    CallExpr*,
    IndexExpr*,
    ArrayExpr*,
    TupleExpr*,
    AssignExpr*,
    LogicalExpr*,
    LambdaExpr*,
    PropertyAccessExpr*,
    SelfExpr*,
    SuperExpr*,
    NewExpr*,
    SetExpr*,
    DictionaryExpr*,
    SpreadExpr*,
    TernaryExpr*,
    AwaitExpr*,
    DestructureAssignExpr*
>;

struct Expr {
    int line;
    int column;
    int length;
    std::string filename;
    ExprVariant variant;
    
    Expr(int line, int column, int length, const std::string& file, ExprVariant var) 
        : line(line), column(column), length(length), filename(file), variant(std::move(var)) {}
};




using StmtVariant = std::variant<
    BlockStmt*,
    ExpressionStmt*,
    VarDeclStmt*,
    OutStmt*,
    IfStmt*,
    WhenStmt*,
    WhileStmt*,
    RepeatStmt*,
    BreakStmt*,
    ContinueStmt*,
    FunctionDeclStmt*,
    ReturnStmt*,
    ClassDeclStmt*,
    InterfaceDeclStmt*,
    ArrayAssignStmt*,
    ImportStmt*,
    ExportStmt*,
    PropertyAssignStmt*,
    TryStmt*,
    ThrowStmt*,
    GetStmt*,
    TupleAssignStmt*,
    StructDeclStmt*,
    ModelDeclStmt*,
    MatchStmt*,
    TaskStmt*,
    GiveStmt*,
    EscapeStmt*,
    SkipStmt*,
    StaticStmt*,
    InterfaceStmt*,
    ModelStmt*,
    StructStmt*,
    UseStmt*
>;

struct Stmt {
    int line;
    int column;
    int length;
    std::string filename;
    StmtVariant variant;
    
    Stmt(int line, int column, int length, const std::string& file, StmtVariant var) 
        : line(line), column(column), length(length), filename(file), variant(std::move(var)) {}
};

#endif // AST_CORE_H
