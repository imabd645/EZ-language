#ifndef BYTECODE_COMPILER_H
#define BYTECODE_COMPILER_H

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include "AST.h"
#include "Bytecode.h"
#include "Value.h"

// ============================================================================
// Bytecode Compiler: AST -> Bytecode
// ============================================================================

class BytecodeCompiler {
public:
    BytecodeCompiler();
    
    // Compile AST statements to bytecode
    CompileResult compile(const std::vector<StmtPtr>& statements);
    
    // Compile single function
    BytecodeFunctionPtr compileFunction(const TaskStmt& task, const std::string& name);
    
    // Virtual File System for packaged standalone executables
    static std::unordered_map<std::string, std::string> virtualFileSystem;
    
private:
    // Current function being compiled
    struct Compiler {
        BytecodeFunctionPtr function;
        Compiler* enclosing;  // Parent compiler for nested functions
        
        // Local variables
        std::vector<Local> locals;
        size_t scopeDepth;
        
        // Upvalues
        std::vector<Upvalue> upvalues;
        
        // Label resolution for jumps
        std::unordered_map<std::string, size_t> labels;
        std::vector<std::pair<size_t, std::string>> pendingJumps;

        // Static variable name mangling
        std::unordered_map<std::string, std::string> statics;

        std::string currentClass;
        std::string currentParentClass;
        
        size_t maxLocals;
        
        bool isHarvesting;  // Force top-level assignments to be locals (for modules)

        Compiler(const std::string& name, size_t arity, Compiler* parent = nullptr)
            : function(std::make_shared<BytecodeFunction>(name, arity)),
              enclosing(parent), scopeDepth(0), currentClass(""), currentParentClass(""),
              maxLocals(0), isHarvesting(false) {}
    };
    
    Compiler* current;
    std::vector<BytecodeFunctionPtr> compiledFunctions;
    
    // Compilation state
    size_t currentLine;
    std::string currentFile;
    bool hadError;
    std::string errorMessage;
    
    // Set of known global identifiers to avoid shadowing
    std::unordered_set<std::string> globals;
    
    // === Expression Compilation ===
    void compileExpr(const ExprPtr& expr);
    void compileLiteral(const LiteralExpr& expr);
    void compileIdentifier(const IdentifierExpr& expr);
    void compileBinary(const BinaryExpr& expr);
    void compileUnary(const UnaryExpr& expr);
    void compileCall(const CallExpr& expr);
    void compileIndex(const IndexExpr& expr);
    void compileArray(const ArrayExpr& expr);
    void compileAssign(const AssignExpr& expr);
    void compileLogical(const LogicalExpr& expr);
    void compileLogicalShortCircuit(const BinaryExpr& expr);
    void compileTernary(const TernaryExpr& expr);
    void compileLambda(const LambdaExpr& expr);
    void compilePropertyAccess(const PropertyAccessExpr& expr);
    void compileSelf(const SelfExpr& expr);
    void compileNew(const NewExpr& expr);
    void compileSet(const SetExpr& expr);
    void compileDictionary(const DictionaryExpr& expr);
    void compileSpread(const SpreadExpr& expr);
    
    // === Statement Compilation ===
    void compileStmt(const StmtPtr& stmt);
    void compileExprStmt(const ExprStmt& stmt);
    void compileOutStmt(const OutStmt& stmt);
    void compileVarDecl(const VarDeclStmt& stmt);
    void compileBlock(const BlockStmt& stmt);
    void compileWhen(const WhenStmt& stmt);
    void compileWhile(const WhileStmt& stmt);
    void compileRepeat(const RepeatStmt& stmt);
    void compileGet(const GetStmt& stmt);
    void compileTask(const TaskStmt& stmt);
    void compileGive(const GiveStmt& stmt);
    void compileUse(const UseStmt& stmt);
    void compileEscape(const EscapeStmt& stmt);
    void compileSkip(const SkipStmt& stmt);
    void compileModel(const ModelStmt& stmt);
    void compileInterface(const InterfaceStmt& stmt);
    void compileStatic(const StaticStmt& stmt);
    void compileTry(const TryStmt& stmt);
    void compileThrow(const ThrowStmt& stmt);
    
    // === Scope Management ===
    void beginScope();
    void endScope();
    size_t addLocal(const std::string& name, bool isConst = false);
    int resolveLocal(const std::string& name);
    int resolveUpvalue(const std::string& name);
    int addUpvalue(size_t index, Upvalue::Type type);
    std::string resolveStatic(const std::string& name);
    void markInitialized();
    
    // === Code Emission ===
    Chunk& currentChunk() { return current->function->chunk; }
    void emitByte(uint8_t byte);
    void emitBytes(uint8_t b1, uint8_t b2);
    void emitOp(OpCode op);
    void emitConstant(const Constant& constant);
    void emitConstant(const Value& value);
    size_t emitJump(OpCode jumpOp);
    void patchJump(size_t offset);
    void emitLoop(size_t loopStart);
    void emitReturn();
    void emitClosure(const TaskStmt& stmt);
    
    // === Helpers ===
    size_t makeConstant(const Constant& constant);
    size_t identifierConstant(const std::string& name);
    void error(const std::string& message);
    void errorAt(const std::string& message, int line);
    void emitLoadSelf();
    
    // === Optimizations ===
    void optimizeLastJump();  // Convert JUMP_IF_FALSE + JUMP to single op
    void foldConstants();     // Compile-time constant folding
    bool isConstant(const ExprPtr& expr, Constant& out);
    
    // === Loop Handling ===
    struct LoopContext {
        size_t start;           // Bytecode offset of loop start
        std::vector<size_t> breaks;    // Jump offsets to patch
        std::vector<size_t> continues; // Loop offsets to patch
    };
    std::vector<LoopContext> loopStack;
    void startLoop();
    void endLoop();
    void emitBreak();
    void emitContinue();
    
    // === Try-Catch ===
    struct TryContext {
        size_t tryStart;
        size_t catchJump;
        std::vector<std::string> catchVars;
    };
    std::vector<TryContext> tryStack;
};

#endif // BYTECODE_COMPILER_H
