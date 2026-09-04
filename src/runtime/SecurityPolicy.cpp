#include "runtime/SecurityPolicy.h"
#include <filesystem>
#include <algorithm>
#include <iostream>

#ifdef _WIN32
#include <io.h>
#define ISATTY(fd) _isatty(fd)
#define FILENO(f) _fileno(f)
#else
#include <unistd.h>
#define ISATTY(fd) isatty(fd)
#define FILENO(f) fileno(f)
#endif

namespace fs = std::filesystem;

bool SecurityPolicy::safeMode = false;
bool SecurityPolicy::allowAll = false;
bool SecurityPolicy::allowFFI = false;
bool SecurityPolicy::allowProcess = false;
bool SecurityPolicy::allowNet = false;
bool SecurityPolicy::allowRead = false;
bool SecurityPolicy::allowWrite = false;
bool SecurityPolicy::noPrompt = false;

std::vector<std::string> SecurityPolicy::allowedReadPaths;
std::vector<std::string> SecurityPolicy::allowedWritePaths;
std::vector<std::string> SecurityPolicy::allowedNetHosts;

void SecurityPolicy::reset() {
    safeMode = false;
    allowAll = false;
    allowFFI = false;
    allowProcess = false;
    allowNet = false;
    allowRead = false;
    allowWrite = false;
    noPrompt = false;
    allowedReadPaths.clear();
    allowedWritePaths.clear();
    allowedNetHosts.clear();
}

bool SecurityPolicy::isInteractiveTerminal() {
    return ISATTY(FILENO(stdin)) && ISATTY(FILENO(stderr));
}

bool SecurityPolicy::promptUser(const std::string& capability, const std::string& detail) {
    if (noPrompt || !isInteractiveTerminal()) return false;

    std::cerr << "\n[Security Prompt] Script requested capability: " << capability;
    if (!detail.empty()) {
        std::cerr << " ('" << detail << "')";
    }
    std::cerr << "\nAllow access? [y]es / [n]o / [a]lways: " << std::flush;

    std::string response;
    if (!std::getline(std::cin, response)) {
        std::cerr << std::endl;
        return false;
    }

    while (!response.empty() && isspace((unsigned char)response.front())) response.erase(response.begin());
    while (!response.empty() && isspace((unsigned char)response.back())) response.pop_back();
    std::transform(response.begin(), response.end(), response.begin(), ::tolower);

    if (response == "y" || response == "yes") {
        return true;
    }
    if (response == "a" || response == "always") {
        if (capability == "FFI") allowFFI = true;
        else if (capability == "PROCESS") allowProcess = true;
        else if (capability == "NET") {
            if (!detail.empty()) allowedNetHosts.push_back(detail);
            else allowNet = true;
        }
        else if (capability == "READ") {
            if (!detail.empty()) allowedReadPaths.push_back(detail);
            else allowRead = true;
        }
        else if (capability == "WRITE") {
            if (!detail.empty()) allowedWritePaths.push_back(detail);
            else allowWrite = true;
        }
        return true;
    }

    return false;
}

static bool isSubpathOrEqual(const fs::path& base, const fs::path& target) {
    try {
        fs::path normBase = fs::weakly_canonical(base);
        fs::path normTarget = fs::weakly_canonical(target);

        auto bIt = normBase.begin();
        auto tIt = normTarget.begin();

        while (bIt != normBase.end() && tIt != normTarget.end()) {
            std::string bPart = bIt->string();
            std::string tPart = tIt->string();
#ifdef _WIN32
            std::transform(bPart.begin(), bPart.end(), bPart.begin(), ::tolower);
            std::transform(tPart.begin(), tPart.end(), tPart.begin(), ::tolower);
#endif
            if (bPart != tPart) return false;
            ++bIt;
            ++tIt;
        }
        return bIt == normBase.end();
    } catch (...) {
        return false;
    }
}

bool SecurityPolicy::checkFFI(RuntimeContext& interp, const std::string& libraryName) {
    if (!safeMode || allowAll || allowFFI) return true;
    if (promptUser("FFI", libraryName)) return true;

    std::string msg = "Native FFI is disabled in --safe mode.";
    if (!libraryName.empty()) {
        msg += " Blocked attempt to load '" + libraryName + "'.";
    }
    msg += " Grant permission with --allow-ffi or --allow-all.";
    interp.throwException("PermissionError", msg, 0, "");
    return false;
}

bool SecurityPolicy::checkProcess(RuntimeContext& interp, const std::string& command) {
    if (!safeMode || allowAll || allowProcess) return true;
    if (promptUser("PROCESS", command)) return true;

    std::string msg = "Process execution (system/exec) is disabled in --safe mode.";
    if (!command.empty()) {
        msg += " Blocked command '" + command + "'.";
    }
    msg += " Grant permission with --allow-process or --allow-all.";
    interp.throwException("PermissionError", msg, 0, "");
    return false;
}

bool SecurityPolicy::checkNet(RuntimeContext& interp, const std::string& targetHostOrUrl) {
    if (!safeMode || allowAll || allowNet) return true;

    if (!allowedNetHosts.empty() && !targetHostOrUrl.empty()) {
        for (const auto& host : allowedNetHosts) {
            if (host == "*" || targetHostOrUrl.find(host) != std::string::npos) {
                return true;
            }
        }
    }

    if (promptUser("NET", targetHostOrUrl)) return true;

    std::string msg = "Network access is disabled in --safe mode.";
    if (!targetHostOrUrl.empty()) {
        msg += " Blocked request to '" + targetHostOrUrl + "'.";
    }
    msg += " Grant permission with --allow-net or --allow-all.";
    interp.throwException("PermissionError", msg, 0, "");
    return false;
}

bool SecurityPolicy::checkRead(RuntimeContext& interp, const std::string& targetPath) {
    if (!safeMode || allowAll || allowRead) return true;

    fs::path target(targetPath);

    if (allowedReadPaths.empty()) {
        if (isSubpathOrEqual(fs::current_path(), target)) return true;
    } else {
        for (const auto& allowed : allowedReadPaths) {
            if (allowed == "*" || isSubpathOrEqual(fs::path(allowed), target)) {
                return true;
            }
        }
    }

    if (promptUser("READ", targetPath)) return true;

    std::string msg = "File read access to '" + targetPath + "' is denied in --safe mode. Grant permission with --allow-read=" + targetPath + " or --allow-all.";
    interp.throwException("PermissionError", msg, 0, "");
    return false;
}

bool SecurityPolicy::checkWrite(RuntimeContext& interp, const std::string& targetPath) {
    if (!safeMode || allowAll || allowWrite) return true;

    fs::path target(targetPath);

    if (!allowedWritePaths.empty()) {
        for (const auto& allowed : allowedWritePaths) {
            if (allowed == "*" || isSubpathOrEqual(fs::path(allowed), target)) {
                return true;
            }
        }
    }

    if (promptUser("WRITE", targetPath)) return true;

    std::string msg = "File write access to '" + targetPath + "' is denied in --safe mode. Grant permission with --allow-write=" + targetPath + " or --allow-all.";
    interp.throwException("PermissionError", msg, 0, "");
    return false;
}
