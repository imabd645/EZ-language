#include "runtime/SecurityPolicy.h"
#include <filesystem>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

bool SecurityPolicy::safeMode = false;
bool SecurityPolicy::allowAll = false;
bool SecurityPolicy::allowFFI = false;
bool SecurityPolicy::allowProcess = false;
bool SecurityPolicy::allowNet = false;
bool SecurityPolicy::allowRead = false;
bool SecurityPolicy::allowWrite = false;

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
    allowedReadPaths.clear();
    allowedWritePaths.clear();
    allowedNetHosts.clear();
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

    std::string msg = "File write access to '" + targetPath + "' is denied in --safe mode. Grant permission with --allow-write=" + targetPath + " or --allow-all.";
    interp.throwException("PermissionError", msg, 0, "");
    return false;
}
