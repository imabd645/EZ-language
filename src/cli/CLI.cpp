#include "cli/CLI.h"
#include "cli/Version.h"
#include "builtins/Builtins.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
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
#include "utils/EzLibPath.h"
#include <map>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <termios.h>
#include <unistd.h>
#include <sys/wait.h>
#else
#include <termios.h>
#include <unistd.h>
#include <sys/wait.h>
#endif
#include <cstdint>

namespace fs = std::filesystem;
bool g_disableContracts = false;
bool g_disableTypeCheck = false;
// Set by --debug. Gates the "Local variables:" dump on an uncaught
// exception's traceback -- off by default because those values can include
// things a script's own output never would (passwords, tokens, request
// bodies), not just because the trace is noisy.
bool g_debugMode = false;

/**
 * Read a line without echoing it, so a password never appears on screen or in
 * a scrollback buffer. Falls back to a normal read if the handle is not a
 * console (a pipe, for instance), because there is nothing to hide there.
 */
static std::string readHiddenLine() {
    std::string line;
#ifdef _WIN32
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (in != INVALID_HANDLE_VALUE && GetConsoleMode(in, &mode)) {
        SetConsoleMode(in, mode & ~ENABLE_ECHO_INPUT);
        std::getline(std::cin, line);
        SetConsoleMode(in, mode);
        std::cout << std::endl;
    } else {
        std::getline(std::cin, line);
    }
#else
    struct termios oldt, newt;
    if (tcgetattr(STDIN_FILENO, &oldt) == 0) {
        newt = oldt;
        newt.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        std::getline(std::cin, line);
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        std::cout << std::endl;
    } else {
        std::getline(std::cin, line);
    }
#endif
    return line;
}

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

#ifdef _WIN32
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
#endif

void runFromSource(const std::string& source, const std::string& path, bool traceExecution = false) {
    // Register source for error reporting (line snippets)
    EZ_RegisterSource(path, source);

    Lexer lexer(source, path);
    std::vector<Token> tokens = lexer.tokenize();
    
    if (lexer.hasError()) {
        exit(65);
    }
    
    ASTArena arena;
    Parser parser(tokens, arena);
    std::vector<StmtPtr> statements = parser.parse();
    
    if (parser.hasError()) {
        exit(65);
    }
    
    auto globalEnv = std::make_shared<Environment>();
    auto vm = std::make_shared<BytecodeVM>(globalEnv);
    vm->traceExecution = traceExecution;
    vm->debugMode = g_debugMode;
    
    std::vector<std::string> builtins;
    for (const auto& pair : globalEnv->variables) builtins.push_back(pair.first);
    // Same list the type checker uses, handed to the compiler so it can warn
    // when a bare assignment would overwrite a builtin. The type checker only
    // sees the entry script; imported modules reach the compiler alone.
    BytecodeCompiler::builtinNames.insert(builtins.begin(), builtins.end());
    
    if (!g_disableTypeCheck) {
        TypeChecker typeChecker;
        if (!typeChecker.check(statements, builtins)) {
            exit(65);
        }
    }
    
    BytecodeCompiler compiler(arena);
    compiler.disableContracts = g_disableContracts;
    
    CompileResult result = compiler.compile(statements);
    if (!result.success) {
        // The compiler formats its own report -- kind, location, source line,
        // detail. Prefixing it with " Error: " here would put a second, less
        // specific label in front of a message that already names its kind.
        std::cerr << "\n" << result.error << std::endl;
        exit(65);
    }

    // Initialize fast global slot array (Issue C: replaces mutex-locked hash lookups)
        vm->initGlobalSlots(result.globalSlotNames);
    
    try {
        vm->execute(result.mainFunction);
        EventLoop::instance().run();
    } catch (const RuntimeError& e) {
        // runtimeError() already printed the formatted error + stack trace
        exit(70); 
    } catch (const std::exception& e) {
        std::cerr << "Internal Error: " << e.what() << std::endl;
        exit(70);
    }
}

void runFile(const std::string& path, bool traceExecution) {
    if (path.length() >= 4 && path.substr(path.length() - 4) == ".ezc") {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open binary file '" << path << "'" << std::endl;
            exit(65);
        }
        
        std::vector<std::string> globalSlots;
        std::shared_ptr<BytecodeFunction> mainFunc;
        try {
            mainFunc = BytecodeSerializer::deserialize(file, globalSlots);
        } catch (const std::exception& e) {
            std::cerr << "Error decoding .ezc file: " << e.what() << std::endl;
            exit(65);
        }
        
        auto globalEnv = std::make_shared<Environment>();
        auto vm = std::make_shared<BytecodeVM>(globalEnv);
        vm->traceExecution = traceExecution;
        vm->debugMode = g_debugMode;
        vm->initGlobalSlots(globalSlots);
        
        try {
            vm->execute(mainFunc);
            EventLoop::instance().run();
        } catch (const RuntimeError& e) {
            exit(70); 
        } catch (const std::exception& e) {
            std::cerr << "Internal Error: " << e.what() << std::endl;
            exit(70);
        }
        return;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file '" << path << "'" << std::endl;
        exit(65);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    
    if (source.size() >= 3 && (unsigned char)source[0] == 0xEF && (unsigned char)source[1] == 0xBB && (unsigned char)source[2] == 0xBF) {
        source = source.substr(3); // Skip UTF-8 BOM
    } else if (source.size() >= 2 && (unsigned char)source[0] == 0xFF && (unsigned char)source[1] == 0xFE) {
        std::cerr << "Error: File '" << path << "' is encoded in UTF-16LE. EZ currently only supports UTF-8. Please save the file as UTF-8 (Without BOM) in your editor." << std::endl;
        exit(65);
    } else if (source.size() >= 2 && (unsigned char)source[0] == 0xFE && (unsigned char)source[1] == 0xFF) {
        std::cerr << "Error: File '" << path << "' is encoded in UTF-16BE. EZ currently only supports UTF-8. Please save the file as UTF-8 (Without BOM) in your editor." << std::endl;
        exit(65);
    }
    
    runFromSource(source, path, traceExecution);
}

#ifdef _WIN32
static volatile bool g_watchInterrupted = false;
static BOOL WINAPI watchCtrlHandler(DWORD fdwCtrlType) {
    if (fdwCtrlType == CTRL_C_EVENT || fdwCtrlType == CTRL_BREAK_EVENT) {
        g_watchInterrupted = true;
        return 1;
    }
    return 0;
}
#else
static volatile sig_atomic_t g_watchInterrupted = 0;
static void watchSigHandler(int) {
    g_watchInterrupted = 1;
}
#endif

static std::map<std::string, fs::file_time_type> getWatchSnapshot(const std::string& scriptPath) {
    std::map<std::string, fs::file_time_type> snapshot;
    std::error_code ec;

    if (fs::exists(scriptPath, ec)) {
        snapshot[fs::absolute(scriptPath, ec).string()] = fs::last_write_time(scriptPath, ec);
    }

    std::string scriptDir = ".";
    size_t lastSlash = scriptPath.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        scriptDir = scriptPath.substr(0, lastSlash);
    }

    std::vector<std::string> dirs = { scriptDir };
    if (scriptDir != "." && scriptDir != "./") dirs.push_back(".");

    for (const auto& d : dirs) {
        if (!fs::exists(d, ec) || !fs::is_directory(d, ec)) continue;
        for (auto it = fs::recursive_directory_iterator(d, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) { ec.clear(); break; }
            if (it->is_regular_file(ec)) {
                std::string pathStr = it->path().string();
                if ((pathStr.size() >= 3 && pathStr.substr(pathStr.size() - 3) == ".ez") ||
                    pathStr.find("package.ez") != std::string::npos) {
                    snapshot[fs::absolute(it->path(), ec).string()] = fs::last_write_time(it->path(), ec);
                }
            }
        }
    }
    return snapshot;
}

static bool checkWatchModified(const std::map<std::string, fs::file_time_type>& prev,
                               std::map<std::string, fs::file_time_type>& current,
                               const std::string& scriptPath,
                               std::string& changedFile) {
    current = getWatchSnapshot(scriptPath);
    for (const auto& [path, time] : current) {
        auto it = prev.find(path);
        if (it == prev.end()) {
            changedFile = path;
            return true;
        }
        if (it->second != time) {
            changedFile = path;
            return true;
        }
    }
    for (const auto& [path, time] : prev) {
        if (current.find(path) == current.end()) {
            changedFile = path;
            return true;
        }
    }
    return false;
}

void runWatch(const std::string& scriptPath, const std::vector<std::string>& extraArgs) {
    std::cout << "[ez watch] Starting watcher for '" << scriptPath << "'..." << std::endl;

    std::string exePath;
#ifdef _WIN32
    char buf[4096];
    if (GetModuleFileNameA(NULL, buf, sizeof(buf))) {
        exePath = buf;
    } else {
        exePath = ezExeDir() + "ez.exe";
    }
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) {
        exePath = buf;
    } else {
        exePath = ezExeDir() + "ez";
    }
#else
    char buf[4096];
    ssize_t count = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (count > 0) {
        buf[count] = '\0';
        exePath = buf;
    } else {
        exePath = ezExeDir() + "ez";
    }
#endif

#ifdef _WIN32
    SetConsoleCtrlHandler(watchCtrlHandler, 1);
#else
    signal(SIGINT, watchSigHandler);
    signal(SIGTERM, watchSigHandler);
#endif

    std::map<std::string, fs::file_time_type> snapshot = getWatchSnapshot(scriptPath);

    while (!g_watchInterrupted) {
        std::cout << "\n[ez watch] Running " << scriptPath << "..." << std::endl;

#ifdef _WIN32
        std::string cmdLine = "\"" + exePath + "\" \"" + scriptPath + "\"";
        for (const auto& arg : extraArgs) {
            cmdLine += " \"" + arg + "\"";
        }

        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
        cmdBuf.push_back('\0');

        BOOL created = CreateProcessA(
            NULL,
            cmdBuf.data(),
            NULL,
            NULL,
            1,
            0,
            NULL,
            NULL,
            &si,
            &pi
        );

        if (!created) {
            std::cerr << "[ez watch] Failed to start process (Error " << GetLastError() << ")" << std::endl;
            return;
        }

        bool childRunning = true;
        std::string changedFile;

        while (!g_watchInterrupted) {
            DWORD waitRes = WaitForSingleObject(pi.hProcess, 200);
            if (waitRes == WAIT_OBJECT_0) {
                DWORD exitCode = 0;
                GetExitCodeProcess(pi.hProcess, &exitCode);
                childRunning = false;
                std::cout << "[ez watch] Process exited with code " << exitCode << ". Waiting for changes..." << std::endl;
                break;
            }

            std::map<std::string, fs::file_time_type> current;
            if (checkWatchModified(snapshot, current, scriptPath, changedFile)) {
                snapshot = current;
                std::cout << "\n[ez watch] File change detected: " << fs::path(changedFile).filename().string() << ". Restarting..." << std::endl;
                TerminateProcess(pi.hProcess, 0);
                WaitForSingleObject(pi.hProcess, 1000);
                childRunning = false;
                break;
            }
        }

        if (childRunning) {
            TerminateProcess(pi.hProcess, 0);
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        while (!g_watchInterrupted && !childRunning) {
            Sleep(200);
            std::map<std::string, fs::file_time_type> current;
            if (checkWatchModified(snapshot, current, scriptPath, changedFile)) {
                snapshot = current;
                std::cout << "\n[ez watch] File change detected: " << fs::path(changedFile).filename().string() << ". Restarting..." << std::endl;
                break;
            }
        }
#else
        pid_t pid = fork();
        if (pid == 0) {
            std::vector<char*> args;
            args.push_back(const_cast<char*>(exePath.c_str()));
            args.push_back(const_cast<char*>(scriptPath.c_str()));
            for (const auto& a : extraArgs) {
                args.push_back(const_cast<char*>(a.c_str()));
            }
            args.push_back(nullptr);
            execv(exePath.c_str(), args.data());
            _exit(127);
        } else if (pid > 0) {
            bool childRunning = true;
            std::string changedFile;

            while (!g_watchInterrupted) {
                int status = 0;
                pid_t res = waitpid(pid, &status, WNOHANG);
                if (res > 0) {
                    childRunning = false;
                    int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
                    std::cout << "[ez watch] Process exited with code " << exitCode << ". Waiting for changes..." << std::endl;
                    break;
                }

                usleep(200000);
                std::map<std::string, fs::file_time_type> current;
                if (checkWatchModified(snapshot, current, scriptPath, changedFile)) {
                    snapshot = current;
                    std::cout << "\n[ez watch] File change detected: " << fs::path(changedFile).filename().string() << ". Restarting..." << std::endl;
                    kill(pid, SIGTERM);
                    waitpid(pid, &status, 0);
                    childRunning = false;
                    break;
                }
            }

            if (childRunning) {
                kill(pid, SIGKILL);
                int status = 0;
                waitpid(pid, &status, 0);
            }

            while (!g_watchInterrupted && !childRunning) {
                usleep(200000);
                std::map<std::string, fs::file_time_type> current;
                if (checkWatchModified(snapshot, current, scriptPath, changedFile)) {
                    snapshot = current;
                    std::cout << "\n[ez watch] File change detected: " << fs::path(changedFile).filename().string() << ". Restarting..." << std::endl;
                    break;
                }
            }
        }
#endif
    }
    std::cout << "\n[ez watch] Stopped." << std::endl;
}

void compileFileToEzc(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file '" << path << "'" << std::endl;
        exit(65);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    
    if (source.size() >= 3 && (unsigned char)source[0] == 0xEF && (unsigned char)source[1] == 0xBB && (unsigned char)source[2] == 0xBF) {
        source = source.substr(3);
    } else if (source.size() >= 2 && (unsigned char)source[0] == 0xFF && (unsigned char)source[1] == 0xFE) {
        std::cerr << "Error: File '" << path << "' is encoded in UTF-16LE. EZ currently only supports UTF-8. Please save the file as UTF-8 (Without BOM) in your editor." << std::endl;
        exit(65);
    } else if (source.size() >= 2 && (unsigned char)source[0] == 0xFE && (unsigned char)source[1] == 0xFF) {
        std::cerr << "Error: File '" << path << "' is encoded in UTF-16BE. EZ currently only supports UTF-8. Please save the file as UTF-8 (Without BOM) in your editor." << std::endl;
        exit(65);
    }
    
    EZ_RegisterSource(path, source);
    Lexer lexer(source, path);
    std::vector<Token> tokens = lexer.tokenize();
    if (lexer.hasError()) exit(65);
    
    ASTArena arena;
    Parser parser(tokens, arena);
    std::vector<StmtPtr> statements = parser.parse();
    if (parser.hasError()) exit(65);
    
    auto globalEnv = std::make_shared<Environment>();
    BytecodeVM vm(globalEnv); // This registers all built-in functions
    
    std::vector<std::string> builtins;
    for (const auto& pair : globalEnv->variables) builtins.push_back(pair.first);
    // Same list the type checker uses, handed to the compiler so it can warn
    // when a bare assignment would overwrite a builtin. The type checker only
    // sees the entry script; imported modules reach the compiler alone.
    BytecodeCompiler::builtinNames.insert(builtins.begin(), builtins.end());
    
    TypeChecker typeChecker;
    if (!typeChecker.check(statements, builtins)) exit(65);
    
    BytecodeCompiler compiler(arena);
    compiler.disableContracts = g_disableContracts;
    CompileResult result = compiler.compile(statements);
    if (!result.success) {
        // The compiler formats its own report -- kind, location, source line,
        // detail. Prefixing it with " Error: " here would put a second, less
        // specific label in front of a message that already names its kind.
        std::cerr << "\n" << result.error << std::endl;
        exit(65);
    }
    
    std::string outPath = path;
    size_t dot = outPath.find_last_of(".");
    if (dot != std::string::npos) outPath = outPath.substr(0, dot);
    outPath += ".ezc";
    
    std::ofstream outFile(outPath, std::ios::binary);
    if (!outFile.is_open()) {
        std::cerr << "Error: Could not open output file '" << outPath << "'" << std::endl;
        exit(65);
    }
    BytecodeSerializer::serialize(result.mainFunction, result.globalSlotNames, outFile);
    std::cout << "Compiled successfully to " << outPath << std::endl;
}

void dumpFileToEzasm(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file '" << path << "'" << std::endl;
        exit(65);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    
    EZ_RegisterSource(path, source);
    Lexer lexer(source, path);
    std::vector<Token> tokens = lexer.tokenize();
    if (lexer.hasError()) exit(65);
    
    ASTArena arena;
    Parser parser(tokens, arena);
    std::vector<StmtPtr> statements = parser.parse();
    if (parser.hasError()) exit(65);
    
    auto globalEnv = std::make_shared<Environment>();
    auto vm = std::make_shared<BytecodeVM>(globalEnv);
    
    std::vector<std::string> builtins;
    for (const auto& pair : globalEnv->variables) builtins.push_back(pair.first);
    // Same list the type checker uses, handed to the compiler so it can warn
    // when a bare assignment would overwrite a builtin. The type checker only
    // sees the entry script; imported modules reach the compiler alone.
    BytecodeCompiler::builtinNames.insert(builtins.begin(), builtins.end());
    
    TypeChecker typeChecker;
    if (!typeChecker.check(statements, builtins)) exit(65);
    
    BytecodeCompiler compiler(arena);
    compiler.disableContracts = g_disableContracts;
    CompileResult result = compiler.compile(statements);
    if (!result.success) {
        // The compiler formats its own report -- kind, location, source line,
        // detail. Prefixing it with " Error: " here would put a second, less
        // specific label in front of a message that already names its kind.
        std::cerr << "\n" << result.error << std::endl;
        exit(65);
    }
    
    std::string outPath = path; 
    size_t dot = outPath.find_last_of(".");
    if (dot != std::string::npos) outPath = outPath.substr(0, dot);
    outPath += ".ezb";
    
    std::ofstream outFile(outPath);
    if (!outFile.is_open()) {
        std::cerr << "Error: Could not open output file '" << outPath << "'" << std::endl;
        exit(65);
    }
    
    auto old_buf = std::cout.rdbuf(outFile.rdbuf());
    
    std::function<void(const std::shared_ptr<BytecodeFunction>&)> dis;
    dis = [&](const std::shared_ptr<BytecodeFunction>& f) {
        f->chunk.disassemble(f->name.empty() ? "main" : f->name, &result.globalSlotNames, &f->nestedFunctions);
        for (const auto& nf : f->nestedFunctions) {
            std::cout << "\n";
            dis(nf);
        }
    };
    
    dis(result.mainFunction);
    
    std::cout.rdbuf(old_buf);
    std::cout << "Disassembled successfully to " << outPath << " (Open this in VS Code to review)" << std::endl;
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

#include <set>

void showVersion() {
    std::cout << "EZ " << EZ_VERSION_STRING << std::endl;
}

void showHelp() {
    std::cout << "EZ Language Interpreter " << EZ_VERSION_STRING << std::endl;
    std::cout << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  ez                Run REPL (interactive mode)" << std::endl;
    std::cout << "  ez <file.ez>      Run a script file" << std::endl;
    std::cout << "  ez -c <code>      Run code passed in as a string" << std::endl;
    std::cout << "  ez --watch <file> Run script in watch mode (auto-reload on save)" << std::endl;
    std::cout << "  ez -w <file>      Short alias for --watch" << std::endl;
    std::cout << "  ez <file.ez> [args...]   Arguments reach the script as argv" << std::endl;
    std::cout << "  ez -c <code> [args...]   Arguments reach the code as argv" << std::endl;
    std::cout << "  ez <file.ez> -- [args...] Pass args through even if they look like flags" << std::endl;
    std::cout << "  ez --version      Print the interpreter version" << std::endl;
    std::cout << std::endl;
    std::cout << "Packages:" << std::endl;
    std::cout << "  ez install                  Install everything in package.ez" << std::endl;
    std::cout << "  ez install <pkg>[@range]    Add and install a package" << std::endl;
    std::cout << "  ez install <pkg> --force    Replace a package ez did not install" << std::endl;
    std::cout << "  ez uninstall <pkg>          Remove a package" << std::endl;
    std::cout << "  ez list                     List installed packages" << std::endl;
    std::cout << "  ez search <query>           Search the registry" << std::endl;
    std::cout << "  ez info <pkg>               Show package details" << std::endl;
    std::cout << "  ez init <name>              Scaffold a new package" << std::endl;
    std::cout << std::endl;
    std::cout << "Publishing:" << std::endl;
    std::cout << "  ez register                 Create a registry account" << std::endl;
    std::cout << "  ez login / logout / whoami  Manage your session" << std::endl;
    std::cout << "  ez publish                  Publish the package in this directory" << std::endl;
    std::cout << "  ez yank <pkg>@<version>     Hide a version from resolution" << std::endl;
    std::cout << "  ez unyank <pkg>@<version>   Undo a yank" << std::endl;
    std::cout << "  ez config registry [url]    Show or set the registry" << std::endl;
    std::cout << std::endl;
    std::cout << "  ez bundle <file.ez> [out.exe] [--gui] [--icon app.ico]  Create a standalone executable" << std::endl;
    std::cout << "  ez --help         Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --trace           Trace bytecode execution" << std::endl;
    std::cout << "  --debug           Show local variables in an uncaught exception's traceback" << std::endl;
    std::cout << "  --no-typecheck    Disable the static type checker" << std::endl;
    std::cout << "  --no-contracts    Disable contract enforcement" << std::endl;
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

// Defined in runtime/Runtime.cpp. String interning is opt-in per thread and only
// the main thread may opt in: the pool is thread_local but the strings it hands
// out escape the thread, so tearing one down at thread exit crashes the process.
void ez_enable_string_interning();

int cli_main(int argc, char* argv[]) {
    // Ensure UTF-8 output for emojis and special characters on Windows consoles
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    // This is the main thread; its pool is destroyed at process exit like any
    // other global, so interning here is safe. Every other thread -- spawn
    // workers, Timer callbacks, and the web server's threads inside
    // http_accel.dll -- leaves interning off and allocates strings directly.
    ez_enable_string_interning();

    signal(SIGSEGV, signalHandler);
    signal(SIGABRT, signalHandler);
    std::set_terminate(terminateHandler);
#ifdef _WIN32
    AddVectoredExceptionHandler(1, VectoredHandler);
#endif

    
    // Check for appended VFS payload
    char exePath[4096];
    bool hasPath = false;
#ifdef _WIN32
    hasPath = GetModuleFileNameA(NULL, exePath, 4096) != 0;
#elif defined(__APPLE__)
    uint32_t pathBufSize = 4096;
    if (_NSGetExecutablePath(exePath, &pathBufSize) == 0) {
        hasPath = true;
    }
#else
    ssize_t count = readlink("/proc/self/exe", exePath, 4096);
    if (count > 0) {
        exePath[count] = '\0';
        hasPath = true;
    }
#endif
    if (hasPath) {
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
                        // A bundled program owns the whole command line: there
                        // is no script path to skip and no interpreter flags to
                        // strip, so every argument is the program's own.
                        g_scriptName = exePath;
                        g_scriptArgs.clear();
                        for (int i = 1; i < argc; i++) g_scriptArgs.push_back(argv[i]);
                        runFromSource(BytecodeCompiler::virtualFileSystem["__main__.ez"], "__main__.ez");
                        return 0;
                    }
                }
            }
        }
    }
    
    if (argc > 1) {
        std::string cmd = argv[1];

        // Answered from the binary alone, so it is checked before anything
        // that touches the filesystem. Without this the argument falls through
        // to the run-a-script branch and fails with
        // "Could not open file '--version'".
        if (cmd == "--version" || cmd == "-v" || cmd == "version") {
            showVersion();
            return 0;
        }

        if (cmd == "-c" || cmd == "--eval" || cmd == "-e") {
            if (argc < 3) {
                std::cerr << "Argument expected for the -c option" << std::endl;
                std::cerr << "Usage: ez -c <code> [args...]" << std::endl;
                return 1;
            }
            std::string code = argv[2];
            g_scriptName = "-c";
            g_scriptArgs.clear();
            bool traceExecution = false;
            bool passThrough = false;
            for (int i = 3; i < argc; i++) {
                std::string arg = argv[i];
                if (passThrough) {
                    g_scriptArgs.push_back(arg);
                } else if (arg == "--") {
                    passThrough = true;
                } else if (arg == "--trace") {
                    traceExecution = true;
                } else if (arg == "--debug") {
                    g_debugMode = true;
                } else if (arg == "--no-contracts") {
                    g_disableContracts = true;
                } else if (arg == "--no-typecheck") {
                    g_disableTypeCheck = true;
                } else {
                    g_scriptArgs.push_back(arg);
                }
            }
            runFromSource(code, "<string>", traceExecution);
            return 0;
        }

        if (cmd == "--watch" || cmd == "-w" || cmd == "watch") {
            if (argc < 3) {
                std::cerr << "Argument expected for watch mode" << std::endl;
                std::cerr << "Usage: ez --watch <file.ez> [args...]" << std::endl;
                return 1;
            }
            std::string targetScript = argv[2];
            std::vector<std::string> extraArgs;
            for (int i = 3; i < argc; i++) {
                extraArgs.push_back(argv[i]);
            }
            runWatch(targetScript, extraArgs);
            return 0;
        }

        if (cmd == "install" || cmd == "i" || cmd == "add") {
            PackageManager pm;
            // Packages install into the shared library root, so --force is what
            // permits replacing a directory ez did not put there.
            std::vector<std::string> names;
            for (int i = 2; i < argc; i++) {
                std::string arg = argv[i];
                if (arg == "--force" || arg == "-f") pm.setForce(true);
                else names.push_back(arg);
            }
            // `ez install` with no package installs what package.ez declares;
            // with one, it adds that package and records it there.
            if (names.empty()) return pm.installAll() ? 0 : 1;
            bool allOk = true;
            for (const auto& name : names) {
                if (!pm.installOne(name)) allOk = false;
            }
            return allOk ? 0 : 1;
        }
        else if (cmd == "uninstall" || cmd == "remove" || cmd == "rm") {
            if (argc < 3) {
                std::cout << "Usage: ez uninstall <package>" << std::endl;
                return 1;
            }
            PackageManager pm;
            return pm.uninstall(argv[2]) ? 0 : 1;
        }
        else if (cmd == "search") {
            if (argc < 3) {
                std::cout << "Usage: ez search <query>" << std::endl;
                return 1;
            }
            std::string query = argv[2];
            for (int i = 3; i < argc; i++) query += " " + std::string(argv[i]);
            PackageManager pm;
            return pm.search(query) ? 0 : 1;
        }
        else if (cmd == "info" || cmd == "show") {
            if (argc < 3) {
                std::cout << "Usage: ez info <package>" << std::endl;
                return 1;
            }
            PackageManager pm;
            return pm.info(argv[2]) ? 0 : 1;
        }
        else if (cmd == "publish") {
            PackageManager pm(".");
            return pm.publish() ? 0 : 1;
        }
        else if (cmd == "yank" || cmd == "unyank") {
            if (argc < 3) {
                std::cout << "Usage: ez " << cmd << " <package>@<version>" << std::endl;
                return 1;
            }
            PackageManager pm;
            return pm.yank(argv[2], cmd == "yank") ? 0 : 1;
        }
        else if (cmd == "login") {
            PackageManager pm;
            std::string user, pass;
            if (argc >= 4) {
                user = argv[2];
                pass = argv[3];
            } else {
                std::cout << "Username: " << std::flush;
                std::getline(std::cin, user);
                std::cout << "Password: " << std::flush;
                pass = readHiddenLine();
            }
            return pm.login(user, pass) ? 0 : 1;
        }
        else if (cmd == "register" || cmd == "signup") {
            PackageManager pm;
            std::string user, email, pass;
            std::cout << "Username: " << std::flush;  std::getline(std::cin, user);
            std::cout << "Email:    " << std::flush;  std::getline(std::cin, email);
            std::cout << "Password: " << std::flush;  pass = readHiddenLine();
            return pm.registerAccount(user, email, pass) ? 0 : 1;
        }
        else if (cmd == "logout") {
            PackageManager pm;
            return pm.logout() ? 0 : 1;
        }
        else if (cmd == "whoami") {
            PackageManager pm;
            return pm.whoami() ? 0 : 1;
        }
        else if (cmd == "config") {
            if (argc >= 4 && std::string(argv[2]) == "registry") {
                if (!ezreg::setRegistryUrl(argv[3])) {
                    std::cerr << "Could not write " << ezreg::configPath() << std::endl;
                    return 1;
                }
                std::cout << "Registry set to " << ezreg::registryUrl() << std::endl;
                return 0;
            }
            if (argc >= 3 && std::string(argv[2]) == "registry") {
                std::cout << ezreg::registryUrl() << std::endl;
                return 0;
            }
            std::cout << "Usage: ez config registry [url]" << std::endl;
            return 1;
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
        else if (cmd == "list" || cmd == "ls") {
            PackageManager pm;
            pm.listInstalled();
            return 0;
        }
        else if (cmd == "bundle") {
            if (argc < 3) {
                std::cout << "Usage: ez bundle <entry_script.ez> [output.exe] [--gui] [--icon app.ico]" << std::endl;
                return 1;
            }
            std::string entryScript = argv[2];
            std::string outputExe;
            bool isGui = false;
            std::string iconPath = "";
            
            for (int i = 3; i < argc; i++) {
                std::string arg = argv[i];
                if (arg == "--gui") {
                    isGui = true;
                } else if (arg == "--icon" && i + 1 < argc) {
                    iconPath = argv[++i];
                } else if (outputExe.empty()) {
                    outputExe = arg;
                }
            }
            
            if (outputExe.empty()) {
                outputExe = entryScript;
                size_t dot = outputExe.find_last_of(".");
                if (dot != std::string::npos) outputExe = outputExe.substr(0, dot);
                outputExe += ".exe";
            }
            return bundleFile(entryScript, outputExe, isGui, iconPath) ? 0 : 1;
        }
        else if (cmd == "--help" || cmd == "-h") {
            showHelp();
            return 0;
        }
        else {
            bool traceExecution = false;
            bool compileToEzc = false;
            bool dumpToEzasm = false;
            bool isWatchMode = false;

            // Anything that is not an interpreter flag belongs to the script.
            //
            // `--` ends interpreter options: everything after it is passed
            // through verbatim, so a program can accept `--trace` of its own
            // without the interpreter swallowing it.
            g_scriptName = cmd;
            g_scriptArgs.clear();
            bool passThrough = false;
            for (int i = 2; i < argc; i++) {
                std::string arg = argv[i];
                if (passThrough) {
                    g_scriptArgs.push_back(arg);
                } else if (arg == "--") {
                    passThrough = true;
                } else if (arg == "--trace") {
                    traceExecution = true;
                } else if (arg == "--debug") {
                    g_debugMode = true;
                } else if (arg == "--watch" || arg == "-w") {
                    isWatchMode = true;
                } else if (arg == "--compile" || arg == "--ezc") {
                    compileToEzc = true;
                } else if (arg == "--dump" || arg == "-d" || arg == "--ezasm") {
                    dumpToEzasm = true;
                } else if (arg == "--no-contracts") {
                    g_disableContracts = true;
                } else if (arg == "--no-typecheck") {
                    g_disableTypeCheck = true;
                } else {
                    g_scriptArgs.push_back(arg);
                }
            }

            if (isWatchMode) {
                std::vector<std::string> watchArgs;
                if (traceExecution) watchArgs.push_back("--trace");
                if (g_debugMode) watchArgs.push_back("--debug");
                if (g_disableContracts) watchArgs.push_back("--no-contracts");
                if (g_disableTypeCheck) watchArgs.push_back("--no-typecheck");
                for (const auto& a : g_scriptArgs) watchArgs.push_back(a);
                runWatch(cmd, watchArgs);
                return 0;
            }

            if (compileToEzc) {
                compileFileToEzc(cmd);
            } else if (dumpToEzasm) {
                dumpFileToEzasm(cmd);
            } else {
                runFile(cmd, traceExecution);
            }
            return 0;
        }
    }
    
    bool traceExecution = false;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--trace") {
            traceExecution = true;
        } else if (arg == "--debug") {
            g_debugMode = true;
        } else if (arg == "--no-contracts") {
            g_disableContracts = true;
        } else if (arg == "--no-typecheck") {
            g_disableTypeCheck = true;
        }
    }
    runRepl(traceExecution);
    return 0;
}
