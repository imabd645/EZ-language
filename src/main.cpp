#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <csignal>
#include "Lexer.h"
#include "Parser.h"
#include "BytecodeInterpreter.h"
#include "PackageManager.h"
#include<windows.h>

void signalHandler(int sig) {
    std::cerr << "\n[FATAL] Signal " << sig << " - segfault or abort" << std::endl;
    _exit(139);
}

void terminateHandler() {
    std::cerr << "\n[FATAL] std::terminate() called!" << std::endl;
    if (auto eptr = std::current_exception()) {
        try { std::rethrow_exception(eptr); }
        catch (const std::exception& e) { std::cerr << "  Reason: " << e.what() << std::endl; }
        catch (...) { std::cerr << "  Unknown exception" << std::endl; }
    }
    _exit(140);
}

static LONG WINAPI VectoredHandler(PEXCEPTION_POINTERS pExInfo) {
    DWORD code = pExInfo->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ILLEGAL_INSTRUCTION) {
        std::cerr << "\n[FATAL] Windows Exception 0x" << std::hex << code << std::dec << std::endl;
        if (code == EXCEPTION_ACCESS_VIOLATION) {
            std::cerr << "  Access violation at address 0x" << std::hex
                      << (uintptr_t)pExInfo->ExceptionRecord->ExceptionInformation[1] << std::dec << std::endl;
        }
        _exit(141);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void runFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file '" << path << "'" << std::endl;
        exit(65);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    
    Lexer lexer(source, path);
    std::vector<Token> tokens = lexer.tokenize();
    
    if (lexer.hasError()) {
        exit(65);
    }
    
    Parser parser(tokens);
    std::vector<StmtPtr> statements = parser.parse();
    
    if (parser.hasError()) {
        exit(65);
    }
    
    BytecodeInterpreter interpreter;
    try {
        interpreter.interpret(statements);
    } catch (const RuntimeError& e) {
        // Some errors are printed by Interpreter::runtimeError, others (like undefined vars) 
        // are thrown directly from Environment/Builtins. We ensure it's always visible.
        std::cerr << "Fatal Error: " << e.what() << std::endl;
        exit(70); 
    } catch (const std::exception& e) {
        std::cerr << "Internal Error: " << e.what() << std::endl;
        exit(70);
    }
}

void runRepl() {
    std::cout << "EZ Language Interpreter v1.0 (Bytecode Mode)" << std::endl;
    std::cout << "Type 'exit' to quit" << std::endl;
    std::cout << std::endl;
    
    BytecodeInterpreter interpreter;
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
            Parser parser(tokens);
            std::vector<StmtPtr> statements = parser.parse();
            
            if (!parser.hasError()) {
                try {
                    interpreter.interpret(statements);
                } catch (const std::exception& e) {
                    std::cerr << "Error: " << e.what() << std::endl;
                }
                multiline.clear();
            }
        }
        openBraces = 0;
    }
    
    std::cout << "Goodbye!" << std::endl;
}

void showHelp() {
    std::cout << "EZ Language Interpreter" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  ez                Run REPL (interactive mode)" << std::endl;
    std::cout << "  ez <file.ez>      Run a script file" << std::endl;
    std::cout << "  ez install <pkg>  Install a package" << std::endl;
    std::cout << "  ez list           List installed packages" << std::endl;
    std::cout << "  ez init <name>    Create a new package" << std::endl;
    std::cout << "  ez --help         Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "EZ Language Syntax:" << std::endl;
    std::cout << "  out \"text\"        Print to console" << std::endl;
    std::cout << "  in                Read input from user" << std::endl;
    std::cout << "  x = 5             Variable assignment" << std::endl;
    std::cout << "  when condition    If statement" << std::endl;
    std::cout << "  other             Else clause" << std::endl;
    std::cout << "  repeat i=0 to 10  For loop" << std::endl;
    std::cout << "  while condition   While loop" << std::endl;
    std::cout << "  get x in arr      Foreach loop" << std::endl;
    std::cout << "  task name()       Function definition" << std::endl;
    std::cout << "  give value        Return from function" << std::endl;
    std::cout << "  escape            Break from loop" << std::endl;
    std::cout << "  skip              Continue to next iteration" << std::endl;
    std::cout << std::endl;
    std::cout << "Built-in Functions:" << std::endl;
    std::cout << "  len, push, pop, str, num, type" << std::endl;
    std::cout << "  substr, split, join, replace, trim" << std::endl;
    std::cout << "  upper, lower, reverse, sort, contains" << std::endl;
    std::cout << "  floor, ceil, abs, sqrt, pow, round" << std::endl;
    std::cout << "  min, max, rand, randint, range" << std::endl;
    std::cout << "  indexOf, slice, print, input" << std::endl;
}

int main(int argc, char* argv[]) {
    signal(SIGSEGV, signalHandler);
    signal(SIGABRT, signalHandler);
    std::set_terminate(terminateHandler);
    AddVectoredExceptionHandler(1, VectoredHandler);
    
    if (argc > 1) {
        std::string cmd = argv[1];
        
        if (cmd == "install") {
            if (argc < 3) {
                std::cout << "Usage: ez install <pkg> [version]" << std::endl;
                return 1;
            }
            std::string pkg = argv[2];
            std::string ver = (argc >= 4) ? argv[3] : "main";
            PackageManager pm;
            return pm.installPackage(pkg, ver) ? 0 : 1;
        }
        else if (cmd == "init") {
            if (argc < 3) {
                std::cout << "Usage: ez init <name>" << std::endl;
                return 1;
            }
            PackageManager pm(".");
            pm.initPackage(argv[2]);
            return 0;
        }
        else if (cmd == "list") {
            PackageManager pm;
            pm.listPackages();
            return 0;
        }
        else if (cmd == "--help" || cmd == "-h") {
            showHelp();
            return 0;
        }
        else {
            runFile(cmd);
            return 0;
        }
    }
    
    runRepl();
    return 0;
}
