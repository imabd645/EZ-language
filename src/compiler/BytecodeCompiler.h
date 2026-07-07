#ifndef BYTECODE_COMPILER_H
#define BYTECODE_COMPILER_H

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include "ast/AST.h"
#include "bytecode/Bytecode.h"
#include "runtime/Value.h"

inline std::string getDirectoryName(const std::string& path) {
    size_t lastSlash = path.find_last_of("\\/");
    if (lastSlash == std::string::npos) return ".";
    return path.substr(0, lastSlash);
}


// ============================================================================
// Bytecode Compiler: AST -> Bytecode
// ============================================================================

class BytecodeCompiler {
public:
    BytecodeCompiler();
    
    // Configuration
    bool disableContracts = false;
    
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
        int compilerId;
        
        bool isHarvesting;  // Force top-level assignments to be locals (for modules)
        bool isCached = false;

        Compiler(const std::string& name, size_t arity, Compiler* parent = nullptr);
    };
    
    Compiler* current;
    std::vector<BytecodeFunctionPtr> compiledFunctions;
    
    // Compilation state
    size_t currentLine;
    std::string currentFile;
    bool hadError;
    std::string errorMessage;
    
    // Global slot table: name -> slot index (Issue C optimization)
    std::unordered_map<std::string, uint16_t> globalSlots;
    uint16_t nextGlobalSlot = 0;
    
    // Current function's contract clauses (threaded from compileFunction → compileGive)
    const std::vector<std::pair<ExprPtr, std::string>>* currentEnsuresClauses = nullptr;
    // Map from old()-expr string key → local slot holding the captured entry value
    std::unordered_map<std::string, size_t> oldCaptures;

    // Allocate or look up a global slot index for a given name.
    // Idempotent: repeated calls for the same name return the same slot.
    uint16_t globalSlotFor(const std::string& name);
    
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
    void compileDestructureAssign(const DestructureAssignExpr& expr);
    void compileLogical(const LogicalExpr& expr);
    void compileLogicalShortCircuit(const BinaryExpr& expr);
    void compileTernary(const TernaryExpr& expr);
    void compileLambda(const LambdaExpr& expr);
    void compilePropertyAccess(const PropertyAccessExpr& expr);
    void compileSelf(const SelfExpr& expr);
    void compileSuper(const SuperExpr& expr);
    void compileNew(const NewExpr& expr);
    void compileTuple(const TupleExpr& expr);
    void compileSet(const SetExpr& expr);
    void compileDictionary(const DictionaryExpr& expr);
    void compileSpread(const SpreadExpr& expr);
    void compileAwait(const AwaitExpr& expr);
    
    // === Statement Compilation ===
    void compileStmt(const StmtPtr& stmt);
    void compileExpressionStmt(const ExpressionStmt& stmt);
    void compileOutStmt(const OutStmt& stmt);
    void compileVarDecl(const VarDeclStmt& stmt);
    void compileBlock(const BlockStmt& stmt);
    void compileWhen(const WhenStmt& stmt);
    void compileWhile(const WhileStmt& stmt);
    void compileRepeat(const RepeatStmt& stmt);
    void compileGet(const GetStmt& stmt);
    void compileMatch(const MatchStmt& stmt);
    void compileTask(const TaskStmt& stmt);
    void compileGive(const GiveStmt& stmt);
    void compileContractChecks(const std::vector<std::pair<ExprPtr, std::string>>& clauses, bool isPrecondition);
    void compileUse(const UseStmt& stmt);
    void compileEscape(const EscapeStmt& stmt);
    void compileSkip(const SkipStmt& stmt);
    void compileModel(const ModelStmt& stmt);
    void compileInterface(const InterfaceStmt& stmt);
    void compileStatic(const StaticStmt& stmt);
    void compileTry(const TryStmt& stmt);
    void compileThrow(const ThrowStmt& stmt);
    void compileExport(const ExportStmt& stmt);
    
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
    void emitClosure(const TaskStmt& stmt, bool isMethod = false);
    
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
