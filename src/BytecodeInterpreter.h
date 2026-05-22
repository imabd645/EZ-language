#ifndef BYTECODE_INTERPRETER_H
#define BYTECODE_INTERPRETER_H

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include "AST.h"
#include "Value.h"
#include "Environment.h"
#include "BytecodeCompiler.h"
#include "BytecodeVM.h"

// ============================================================================
// Hybrid Interpreter: AST walker + Bytecode VM
// 
// This class provides a seamless integration between the existing AST-based
// interpreter and the new bytecode VM. It can:
// 1. Interpret AST directly (legacy mode)
// 2. Compile to bytecode and execute (fast mode)
// 3. Cache bytecode for reuse (performance mode)
// ============================================================================

enum class ExecutionMode {
    AST_ONLY,           // Use AST interpreter only (compatibility mode)
    BYTECODE_ONLY,      // Use bytecode compiler + VM only
    HYBRID_SMART,     // Auto-select based on complexity
    HYBRID_CACHED     // Cache bytecode for repeated execution
};

class BytecodeInterpreter {
public:
    BytecodeInterpreter();
    explicit BytecodeInterpreter(std::shared_ptr<Environment> startEnv);
    
    // Configuration
    void setExecutionMode(ExecutionMode mode);
    ExecutionMode getExecutionMode() const { return mode; }
    void enableBytecodeCache(bool enable);
    bool isBytecodeCacheEnabled() const { return useBytecodeCache; }
    
    // Main interpretation entry point
    void interpret(const std::vector<StmtPtr>& statements);
    
    // Evaluate single expression (for REPL)
    Value evaluateExpression(const ExprPtr& expr);
    
    // Execute compiled bytecode directly
    Value executeBytecode(BytecodeFunctionPtr function);
    
    // Compile and cache
    BytecodeFunctionPtr compile(const std::vector<StmtPtr>& statements);
    BytecodeFunctionPtr compileFunction(const TaskStmt& task);
    
    // Cache management
    void clearCache();
    size_t getCacheSize() const;
    void dumpCacheStats() const;
    
    // Performance metrics
    struct Metrics {
        size_t astExecutions;
        size_t bytecodeCompilations;
        size_t bytecodeExecutions;
        size_t cacheHits;
        double totalCompileTime;
        double totalASTTime;
        double totalBytecodeTime;
    };
    Metrics getMetrics() const { return metrics; }
    void resetMetrics();
    void printMetrics() const;
    
    // Access to underlying interpreter/VM for advanced use
    std::shared_ptr<Environment> getGlobalEnv() const { return globalEnv; }
    void defineGlobal(const std::string& name, const Value& value);
    
    // Call function from native code
    Value callFunction(const Value& callee, const std::vector<Value>& args, 
                      int line = 0, const std::string& filename = "");
    
    // String conversion
    std::string stringify(const Value& val, int line = 0, const std::string& filename = "");
    
private:
    ExecutionMode mode;
    bool useBytecodeCache;
    
    // Components
    std::shared_ptr<Environment> globalEnv;
    std::unique_ptr<BytecodeCompiler> compiler;
    std::unique_ptr<BytecodeVM> vm;
    
    // Cache: AST signature -> compiled bytecode
    struct CacheEntry {
        BytecodeFunctionPtr function;
        size_t useCount;
        double avgExecutionTime;
        size_t lastAccess;
    };
    std::unordered_map<size_t, CacheEntry> bytecodeCache;
    size_t cacheAccessCounter;
    static constexpr size_t MAX_CACHE_SIZE = 64;
    static constexpr size_t CACHE_MIN_USE_COUNT = 2; // Min uses to keep in cache
    
    // Metrics
    Metrics metrics;
    
    // AST signature generation (for caching)
    size_t computeASTSignature(const std::vector<StmtPtr>& statements);
    
    // Execution methods
    void executeAST(const std::vector<StmtPtr>& statements);
    void executeBytecodeMode(const std::vector<StmtPtr>& statements);
    Value executeASTExpression(const ExprPtr& expr);
    Value executeBytecodeWithMetrics(BytecodeFunctionPtr function);
    
    // Smart mode selection
    bool shouldUseBytecode(const std::vector<StmtPtr>& statements);
    int estimateComplexity(const std::vector<StmtPtr>& statements);
    
    // Cache management
    void addToCache(size_t signature, BytecodeFunctionPtr function);
    BytecodeFunctionPtr getFromCache(size_t signature);
    void evictFromCache();
    
    // Initialization
    void initComponents();
    void initBuiltins();
};

// ============================================================================
// Convenience factory functions
// ============================================================================

// Create interpreter with bytecode enabled (recommended)
inline std::unique_ptr<BytecodeInterpreter> makeFastInterpreter() {
    auto interp = std::make_unique<BytecodeInterpreter>();
    interp->setExecutionMode(ExecutionMode::HYBRID_CACHED);
    interp->enableBytecodeCache(true);
    return interp;
}

// Create compatible interpreter (AST only)
inline std::unique_ptr<BytecodeInterpreter> makeCompatibleInterpreter() {
    auto interp = std::make_unique<BytecodeInterpreter>();
    interp->setExecutionMode(ExecutionMode::AST_ONLY);
    return interp;
}

#endif // BYTECODE_INTERPRETER_H
