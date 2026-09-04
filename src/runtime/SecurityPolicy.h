#ifndef SECURITY_POLICY_H
#define SECURITY_POLICY_H

#include <string>
#include <vector>
#include "runtime/RuntimeContext.h"

class SecurityPolicy {
public:
    static bool safeMode;
    static bool allowAll;
    static bool allowFFI;
    static bool allowProcess;
    static bool allowNet;
    static bool allowRead;
    static bool allowWrite;

    static std::vector<std::string> allowedReadPaths;
    static std::vector<std::string> allowedWritePaths;
    static std::vector<std::string> allowedNetHosts;

    static void reset();

    // Permission checks: return true if allowed, or throw PermissionError via interp and return false.
    static bool checkFFI(RuntimeContext& interp, const std::string& libraryName = "");
    static bool checkProcess(RuntimeContext& interp, const std::string& command = "");
    static bool checkNet(RuntimeContext& interp, const std::string& targetHostOrUrl = "");
    static bool checkRead(RuntimeContext& interp, const std::string& targetPath);
    static bool checkWrite(RuntimeContext& interp, const std::string& targetPath);
};

#endif // SECURITY_POLICY_H
