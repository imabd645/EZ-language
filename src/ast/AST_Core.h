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

using ExprPtr = std::shared_ptr<Expr>;
using StmtPtr = std::shared_ptr<Stmt>;

struct TypeAST;
using TypeASTPtr = std::shared_ptr<TypeAST>;

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

using ExprVariant = std::variant<
    std::shared_ptr<LiteralExpr>,
    std::shared_ptr<IdentifierExpr>,
    std::shared_ptr<BinaryExpr>,
    std::shared_ptr<UnaryExpr>,
    std::shared_ptr<CallExpr>,
    std::shared_ptr<IndexExpr>,
    std::shared_ptr<ArrayExpr>,
    std::shared_ptr<TupleExpr>,
    std::shared_ptr<AssignExpr>,
    std::shared_ptr<LogicalExpr>,
    std::shared_ptr<LambdaExpr>,
    std::shared_ptr<PropertyAccessExpr>,
    std::shared_ptr<SelfExpr>,
    std::shared_ptr<SuperExpr>,
    std::shared_ptr<NewExpr>,
    std::shared_ptr<SetExpr>,
    std::shared_ptr<DictionaryExpr>,
    std::shared_ptr<SpreadExpr>,
    std::shared_ptr<TernaryExpr>,
    std::shared_ptr<AwaitExpr>,
    std::shared_ptr<DestructureAssignExpr>
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



#endif // AST_CORE_H
