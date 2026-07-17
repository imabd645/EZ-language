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
#include <windows.h>
#include <cstdint>





void runRepl(bool traceExecution) {
    std::cout << "EZ Language Interpreter v1.0 (Bytecode Mode)" << std::endl;
    std::cout << "Type 'exit' to quit" << std::endl;
    std::cout << std::endl;
    
    auto globalEnv = std::make_shared<Environment>();
    auto vm = std::make_shared<BytecodeVM>(globalEnv);
    vm->traceExecution = traceExecution;
    ASTArena arena;
    BytecodeCompiler compiler(arena);
    compiler.disableContracts = g_disableContracts;
    std::string line;
    std::string multiline;
    int openBraces = 0;
    
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
                if (!g_disableTypeCheck) {
                    TypeChecker typeChecker;
                    typeCheckOk = typeChecker.check(statements, builtins);
                }
                if (typeCheckOk) {
                    try {
                        CompileResult result = compiler.compile(statements);
                        if (result.success) {
                            vm->execute(result.mainFunction);
                        } else {
                            std::cerr << "Compile error: " << result.error << std::endl;
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Error: " << e.what() << std::endl;
                    }
                }
                multiline.clear();
            }
        }
        openBraces = 0;
    }
    
    std::cout << "Goodbye!" << std::endl;
}

