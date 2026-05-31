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
// Bytecode Engine
// 
// This class provides integration between the AST frontend
// and the new bytecode VM.
// ============================================================================

class BytecodeInterpreter {
public:
    BytecodeInterpreter();
    explicit BytecodeInterpreter(std::shared_ptr<Environment> startEnv);
    
    // Configuration
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
        size_t bytecodeCompilations;
        size_t bytecodeExecutions;
        size_t cacheHits;
        double totalCompileTime;
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
    void executeBytecodeMode(const std::vector<StmtPtr>& statements);
    Value executeBytecodeWithMetrics(BytecodeFunctionPtr function);
    
    // Cache management
    void addToCache(size_t signature, BytecodeFunctionPtr function);
    BytecodeFunctionPtr getFromCache(size_t signature);
    void evictFromCache();
    
    // Initialization
    void initComponents();
    void initBuiltins();
};

#endif // BYTECODE_INTERPRETER_H
