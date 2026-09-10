#include "cli/CLI.h"
extern bool g_disableContracts;
extern bool g_disableTypeCheck;
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <csignal>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include "runtime/Value.h"
#include "gc/CycleCollector.h"
#include "eventloop/EventLoop.h"
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/ASTArena.h"
#include "typechecker/TypeChecker.h"
#include "vm/BytecodeVM.h"
#include "compiler/BytecodeCompiler.h"
#include "bytecode/serializer/BytecodeSerializer.h"
#include "cli/PackageManager.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <cstdint>





void runRepl(bool traceExecution) {
    std::cout << "EZ Language Interpreter" << std::endl;
    std::cout << "Type 'exit' to quit" << std::endl;
    std::cout << std::endl;
    
    auto globalEnv = std::make_shared<Environment>();
    auto vm = std::make_shared<BytecodeVM>(globalEnv);
    vm->traceExecution = traceExecution;
    ASTArena arena;
    BytecodeCompiler compiler(arena);
    compiler.disableContracts = g_disableContracts;

    // Same list CLI.cpp hands the compiler for its one-shot script run
    // (see runFile()): lets warnBuiltinAssignment() warn when a bare
    // `readFile = ...` is about to replace a builtin, instead of doing so
    // silently. Builtin names are fixed at VM construction, so this only
    // needs computing once, not every line.
    {
        std::vector<std::string> builtinInit;
        for (const auto& pair : globalEnv->variables) builtinInit.push_back(pair.first);
        BytecodeCompiler::builtinNames.insert(builtinInit.begin(), builtinInit.end());
    }

    std::string line;
    std::string multiline;
    int openBraces = 0;
    // Top-level REPL vars/functions compile to STORE_GLOBAL_SLOT, which never
    // touches globalEnv->variables (that map only holds pre-registered
    // builtins). Without also tracking each one's type here, the
    // typechecker's symbol table forgets every variable the moment the next
    // line is checked, so `x = 0` followed by `x == 1` reports "x used
    // before defined" even though the assignment already ran. This is kept
    // separate from `builtins` below (which declares everything as type
    // "Task") so a plain variable like `x` is typechecked as its real type
    // instead of as a callable.
    std::unordered_map<std::string, TypeInfo> replVarTypes;
    
    while (true) {
        if (openBraces > 0) {
            std::cout << "... ";
        } else {
            std::cout << ">>> ";
        }
        
        if (!std::getline(std::cin, line)) {
            break;
        }
        
        if (line == "exit" && openBraces == 0) {
            break;
        }
        
        // Count braces for multiline input
        for (char c : line) {
            if (c == '{') openBraces++;
            if (c == '}') openBraces--;
        }
        
        multiline += line + "\n";
        
        if (openBraces > 0) {
            continue;
        }
        
        // Process the input
        Lexer lexer(multiline, "repl");
        std::vector<Token> tokens = lexer.tokenize();
        
        if (!lexer.hasError()) {
            Parser parser(tokens, arena);
            std::vector<StmtPtr> statements = parser.parse();
            
            if (!parser.hasError()) {
                std::vector<std::string> builtins;
                for (const auto& pair : globalEnv->variables) builtins.push_back(pair.first);
                
                bool typeCheckOk = true;
                std::unordered_map<std::string, TypeInfo> updatedTypes;
                if (!g_disableTypeCheck) {
                    TypeChecker typeChecker;
                    typeCheckOk = typeChecker.check(statements, builtins, replVarTypes);
                    // Snapshot now: `typeChecker` is scoped to this block, and we
                    // only want to keep this statement's types if it goes on to
                    // compile and execute cleanly below.
                    updatedTypes = typeChecker.declaredGlobals();
                }
                if (typeCheckOk) {
                    try {
                        CompileResult result = compiler.compile(statements);
                        if (result.success) {
                            // The compiler's global slot table is cumulative across
                            // REPL iterations (globalSlots persists on `compiler`),
                            // but the VM's backing storage (globalEnv->globalSlots)
                            // is only grown when we tell it to. growGlobalSlots()
                            // only fills in newly-added slots — unlike
                            // initGlobalSlots(), it never re-seeds a slot that
                            // already has a name, so a builtin the user has already
                            // reassigned (e.g. `readFile = ||{...}`) isn't clobbered
                            // back to the original builtin on the next line.
                            vm->growGlobalSlots(result.globalSlotNames);
                            vm->execute(result.mainFunction);
                            if (!g_disableTypeCheck) {
                                // declaredGlobals() includes the `builtins` entries
                                // too (declareVariable puts everything into the
                                // same table) — skip those here since `builtins`
                                // is already rebuilt fresh from globalEnv every
                                // line; only REPL-declared names need carrying
                                // forward.
                                std::unordered_set<std::string> builtinSet(builtins.begin(), builtins.end());
                                for (auto& [name, type] : updatedTypes) {
                                    if (!builtinSet.count(name)) replVarTypes[name] = type;
                                }
                            }
                        } else {
                            std::cerr << "Compile error: " << result.error << std::endl;
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Error: " << e.what() << std::endl;
                    }
                }
                // else: TypeChecker::check() already printed its own
                // "Type Error at line X, column Y" message directly.
            } else {
                // Parser::parse() already printed its own "Error: ..." message
                // (Parser.cpp), same as the lexer below. Nothing more to print
                // here — just make sure we don't keep the broken input around.
            }
        } else {
            // Lexer::tokenize() already printed its own "Error: ..." message
            // (Lexer.cpp) at the point it hit the bad token. Nothing more to
            // print here — just make sure we don't keep the broken input
            // around.
        }
        // Always reset here, success or failure: multiline previously only
        // got cleared on the successful-parse path (nested inside
        // `if (!parser.hasError())`), so a lex or parse error left the bad
        // text sitting in `multiline` forever. Every later line then got
        // appended to that same broken buffer and re-hit the SAME error
        // (e.g. an unterminated string never got closed, so it kept
        // reporting "Unterminated string" at the original line/column no
        // matter what valid code was typed afterward).
        multiline.clear();
        openBraces = 0;
    }
    
    std::cout << "Goodbye!" << std::endl;
}

