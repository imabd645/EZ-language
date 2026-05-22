#include "BytecodeInterpreter.h"
#include <iostream>
#include <chrono>
#include <stdexcept>
#include <algorithm>

// ============================================================================
// BytecodeInterpreter — Native Bytecode Engine
// ============================================================================

BytecodeInterpreter::BytecodeInterpreter()
    : mode(ExecutionMode::BYTECODE_ONLY), useBytecodeCache(true),
      cacheAccessCounter(0) {
    initComponents();
}

BytecodeInterpreter::BytecodeInterpreter(std::shared_ptr<Environment> startEnv)
    : mode(ExecutionMode::BYTECODE_ONLY), useBytecodeCache(true),
      globalEnv(startEnv), cacheAccessCounter(0) {
    initComponents();
}

void BytecodeInterpreter::initComponents() {
    if (!globalEnv) {
        globalEnv = std::make_shared<Environment>();
    }
    
    compiler = std::make_unique<BytecodeCompiler>();
    vm       = std::make_unique<BytecodeVM>(globalEnv);
    
    // Builtins (base + GUI) are now registered by BytecodeVM's constructor
    // via initBuiltins(), so no need to register here.
    
    metrics  = {};
}

void BytecodeInterpreter::setExecutionMode(ExecutionMode m) { mode = m; }
void BytecodeInterpreter::enableBytecodeCache(bool enable) { useBytecodeCache = enable; }

// ============================================================================
// Main Entry Point
// ============================================================================

void BytecodeInterpreter::interpret(const std::vector<StmtPtr>& statements) {
    // Phase 2: All execution is now bytecode-native
    executeBytecodeMode(statements);
}

void BytecodeInterpreter::executeBytecodeMode(const std::vector<StmtPtr>& statements) {
    metrics.bytecodeCompilations++;

    auto t0 = std::chrono::steady_clock::now();
    CompileResult result = compiler->compile(statements);
    auto t1 = std::chrono::steady_clock::now();
    metrics.totalCompileTime +=
        std::chrono::duration<double>(t1 - t0).count();

    if (!result.success) {
        std::cerr << "[BytecodeCompiler] Error: " << result.error << std::endl;
        throw RuntimeError("Compilation failed: " + result.error);
    }

    auto t2 = std::chrono::steady_clock::now();
    try {
        vm->execute(result.mainFunction);
    } catch (const RuntimeError& e) {
        throw; // re-throw for main.cpp to handle
    } catch (const std::exception& e) {
        throw RuntimeError(e.what());
    }
    auto t3 = std::chrono::steady_clock::now();
    metrics.totalBytecodeTime +=
        std::chrono::duration<double>(t3 - t2).count();
    metrics.bytecodeExecutions++;
}

// ============================================================================
// Expression Evaluation (REPL)
// ============================================================================

Value BytecodeInterpreter::evaluateExpression(const ExprPtr& expr) {
    // Compilation of a single expression
    auto stmt = std::make_shared<Stmt>(expr->line, expr->filename, std::make_shared<ExprStmt>(expr));
    CompileResult result = compiler->compile({stmt});
    
    if (!result.success) {
        throw RuntimeError("Expression compile error: " + result.error);
    }
    
    return vm->execute(result.mainFunction);
}

// ============================================================================
// Bytecode compile/exec
// ============================================================================

BytecodeFunctionPtr BytecodeInterpreter::compile(
    const std::vector<StmtPtr>& statements) {
    CompileResult result = compiler->compile(statements);
    if (!result.success) return nullptr;
    return result.mainFunction;
}

BytecodeFunctionPtr BytecodeInterpreter::compileFunction(const TaskStmt& task) {
    return compiler->compileFunction(task, task.name);
}

Value BytecodeInterpreter::executeBytecode(BytecodeFunctionPtr function) {
    return vm->execute(function);
}

// ============================================================================
// Cache
// ============================================================================

void BytecodeInterpreter::clearCache() { bytecodeCache.clear(); }
size_t BytecodeInterpreter::getCacheSize() const { return bytecodeCache.size(); }

// ============================================================================
// Metrics
// ============================================================================

void BytecodeInterpreter::resetMetrics() { metrics = {}; }

void BytecodeInterpreter::printMetrics() const {
    std::cout << "=== BytecodeInterpreter Metrics ===" << std::endl;
    std::cout << "AST executions:           " << metrics.astExecutions       << std::endl;
    std::cout << "Bytecode compilations:    " << metrics.bytecodeCompilations<< std::endl;
    std::cout << "Bytecode executions:      " << metrics.bytecodeExecutions  << std::endl;
    std::cout << "Cache hits:               " << metrics.cacheHits           << std::endl;
    std::cout << "Compile time (s):         " << metrics.totalCompileTime    << std::endl;
    std::cout << "AST time (s):             " << metrics.totalASTTime        << std::endl;
    std::cout << "Bytecode time (s):        " << metrics.totalBytecodeTime   << std::endl;
}

// ============================================================================
// Globals / Call
// ============================================================================

void BytecodeInterpreter::defineGlobal(const std::string& name,
                                        const Value& value) {
    globalEnv->define(name, value);
}

Value BytecodeInterpreter::callFunction(const Value& callee,
                                         const std::vector<Value>& args,
                                         int /*line*/,
                                         const std::string& /*filename*/) {
    return vm->callFunction(callee, args);
}

std::string BytecodeInterpreter::stringify(const Value& val,
                                            int /*line*/,
                                            const std::string& /*filename*/) {
    return val.toString();
}

void BytecodeInterpreter::dumpCacheStats() const {
    std::cout << "Cache entries: " << bytecodeCache.size() << std::endl;
}
