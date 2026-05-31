#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <csignal>
#include <algorithm>
#include "Lexer.h"
#include "Parser.h"
#include "BytecodeInterpreter.h"
#include "BytecodeCompiler.h"
#include "PackageManager.h"
#include <windows.h>
#include <cstdint>

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

void runFromSource(const std::string& source, const std::string& path) {
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
    
    // Use BytecodeInterpreter for faster execution
    BytecodeInterpreter interpreter;
    interpreter.setExecutionMode(ExecutionMode::BYTECODE_ONLY);  // Force bytecode for max speed
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

void runFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file '" << path << "'" << std::endl;
        exit(65);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    
    runFromSource(source, path);
}

bool patchPESubsystem(const std::string& exePath, uint16_t newSubsystem) {
    std::fstream file(exePath, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) return false;
    
    // Read DOS header e_lfanew
    int32_t e_lfanew = 0;
    file.seekg(0x3C);
    file.read((char*)&e_lfanew, 4);
    
    // Check PE signature
    file.seekg(e_lfanew);
    char signature[4];
    file.read(signature, 4);
    if (signature[0] != 'P' || signature[1] != 'E') return false; // Not a valid PE file
    
    // The Subsystem field is at offset 68 into the Optional Header
    // e_lfanew + 4 (Signature) + 20 (COFF Header) + 68 = e_lfanew + 92
    file.seekg(e_lfanew + 92);
    file.write((char*)&newSubsystem, 2);
    return true;
}

bool bundleFile(const std::string& entryScript, const std::string& outputExe, bool isGui) {
    char exePath[MAX_PATH];
    if (!GetModuleFileNameA(NULL, exePath, MAX_PATH)) {
        std::cerr << "Error: Could not determine executable path." << std::endl;
        return false;
    }
    
    std::ifstream entryFile(entryScript, std::ios::binary);
    if (!entryFile.is_open()) {
        std::cerr << "Error: Could not open entry script '" << entryScript << "'" << std::endl;
        return false;
    }
    
    std::cout << "Packaging " << entryScript << " into " << outputExe << "..." << std::endl;
    
    struct PackFile {
        std::string vfsName;
        std::string content;
    };
    std::vector<PackFile> filesToPack;
    
    // 1. Pack main script
    std::stringstream buffer;
    buffer << entryFile.rdbuf();
    filesToPack.push_back({"__main__.ez", buffer.str()});
    std::cout << "  -> Packed __main__.ez (" << buffer.str().length() << " bytes)\n";
    
    // 2. Pack everything in lib/ directory
    std::string baseDir = exePath;
    size_t lastSlash = baseDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        baseDir = baseDir.substr(0, lastSlash);
    }
    std::string libDir = baseDir + "/lib";
    
    if (std::filesystem::exists(libDir)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(libDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".ez") {
                std::string fullPath = entry.path().string();
                std::replace(fullPath.begin(), fullPath.end(), '\\', '/');
                
                std::string vfsName = fullPath.substr(baseDir.length() + 1); // e.g. "lib/gui.ez"
                
                std::ifstream libFile(fullPath, std::ios::binary);
                if (libFile.is_open()) {
                    std::stringstream libBuf;
                    libBuf << libFile.rdbuf();
                    filesToPack.push_back({vfsName, libBuf.str()});
                    std::cout << "  -> Packed " << vfsName << " (" << libBuf.str().length() << " bytes)\n";
                }
            }
        }
    }
    
    // 3. Pack everything in C:/ezlib/ directory
    std::string ezlibDir = "C:/ezlib";
    if (std::filesystem::exists(ezlibDir)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(ezlibDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".ez") {
                std::string fullPath = entry.path().string();
                std::replace(fullPath.begin(), fullPath.end(), '\\', '/');
                
                std::string vfsName = fullPath; // e.g. "C:/ezlib/package/main.ez"
                
                std::ifstream libFile(fullPath, std::ios::binary);
                if (libFile.is_open()) {
                    std::stringstream libBuf;
                    libBuf << libFile.rdbuf();
                    filesToPack.push_back({vfsName, libBuf.str()});
                    std::cout << "  -> Packed " << vfsName << " (" << libBuf.str().length() << " bytes)\n";
                }
            }
        }
    }
    
    // 4. Build VFS Blob
    std::string vfsBlob;
    uint32_t numFiles = filesToPack.size();
    vfsBlob.append((char*)&numFiles, 4);
    
    for (const auto& f : filesToPack) {
        uint32_t nameLen = f.vfsName.length();
        vfsBlob.append((char*)&nameLen, 4);
        vfsBlob.append(f.vfsName);
        
        uint32_t fileLen = f.content.length();
        vfsBlob.append((char*)&fileLen, 4);
        vfsBlob.append(f.content);
    }
    
    // 4. Copy ez.exe to outputExe
    try {
        std::filesystem::copy_file(exePath, outputExe, std::filesystem::copy_options::overwrite_existing);
    } catch (const std::exception& e) {
        std::cerr << "Error copying executable: " << e.what() << std::endl;
        return false;
    }
    
    // 5. Append VFS Blob
    std::ofstream outFile(outputExe, std::ios::binary | std::ios::app);
    if (!outFile.is_open()) {
        std::cerr << "Error: Could not open output file for appending." << std::endl;
        return false;
    }
    
    outFile.write(vfsBlob.data(), vfsBlob.length());
    uint32_t vfsSize = vfsBlob.length();
    outFile.write((char*)&vfsSize, 4);
    outFile.write("EZPKV1", 6);
    outFile.close();
    
    // 6. Patch PE Header if GUI mode is requested
    if (isGui) {
        // 2 = IMAGE_SUBSYSTEM_WINDOWS_GUI
        if (patchPESubsystem(outputExe, 2)) {
            std::cout << "  -> Patched PE Subsystem for GUI Mode (Console Hidden)\n";
        } else {
            std::cout << "  -> Warning: Failed to patch PE Subsystem\n";
        }
    }
    
    std::cout << "\nSuccess! Created standalone executable: " << outputExe << std::endl;
    return true;
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
    std::cout << "  ez bundle <file.ez> [out.exe] [--gui]  Create a standalone executable" << std::endl;
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
    
    // Check for appended VFS payload
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH)) {
        std::ifstream exeFile(exePath, std::ios::binary | std::ios::ate);
        if (exeFile.is_open()) {
            std::streamsize size = exeFile.tellg();
            if (size > 10) {
                exeFile.seekg(-10, std::ios::end);
                char magic[7] = {0};
                uint32_t vfsSize = 0;
                exeFile.read((char*)&vfsSize, 4);
                exeFile.read(magic, 6);
                if (std::string(magic) == "EZPKV1") {
                    exeFile.seekg(-10 - (std::streamoff)vfsSize, std::ios::end);
                    uint32_t numFiles = 0;
                    exeFile.read((char*)&numFiles, 4);
                    for (uint32_t i = 0; i < numFiles; i++) {
                        uint32_t nameLen = 0;
                        exeFile.read((char*)&nameLen, 4);
                        std::string name(nameLen, '\0');
                        exeFile.read(&name[0], nameLen);
                        
                        uint32_t fileLen = 0;
                        exeFile.read((char*)&fileLen, 4);
                        std::string content(fileLen, '\0');
                        exeFile.read(&content[0], fileLen);
                        
                        BytecodeCompiler::virtualFileSystem[name] = content;
                    }
                    if (BytecodeCompiler::virtualFileSystem.count("__main__.ez")) {
                        runFromSource(BytecodeCompiler::virtualFileSystem["__main__.ez"], "__main__.ez");
                        return 0;
                    }
                }
            }
        }
    }
    
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
        else if (cmd == "bundle") {
            if (argc < 3) {
                std::cout << "Usage: ez bundle <entry_script.ez> [output.exe] [--gui]" << std::endl;
                return 1;
            }
            std::string entryScript = argv[2];
            std::string outputExe;
            bool isGui = false;
            
            for (int i = 3; i < argc; i++) {
                std::string arg = argv[i];
                if (arg == "--gui") isGui = true;
                else if (outputExe.empty()) outputExe = arg;
            }
            
            if (outputExe.empty()) {
                outputExe = entryScript;
                size_t dot = outputExe.find_last_of(".");
                if (dot != std::string::npos) outputExe = outputExe.substr(0, dot);
                outputExe += ".exe";
            }
            return bundleFile(entryScript, outputExe, isGui) ? 0 : 1;
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
