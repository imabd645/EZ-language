#ifndef PERMISSION_SCANNER_H
#define PERMISSION_SCANNER_H

#include <string>
#include <vector>
#include <set>
#include "ast/AST.h"

struct PermissionFinding {
    std::string type;       // "FFI", "PROCESS", "NET", "READ", "WRITE"
    std::string operation;  // e.g. "os_load_lib", "system", "http_get", "readFile", "writeFile"
    std::string target;     // e.g. "Kernel32.dll", "https://example.com", "<dynamic>"
    std::string file;
    int line = 0;
    int column = 0;
};

struct ImportedPackageInfo {
    std::string name;
    std::string version;
    std::string path;
    std::vector<std::string> declaredPermissions;
};

struct PermissionReport {
    std::string targetScript;
    std::vector<PermissionFinding> findings;
    std::vector<ImportedPackageInfo> packages;
    std::set<std::string> scannedFiles;

    bool needsFFI = false;
    bool needsProcess = false;
    std::set<std::string> netHosts;
    bool netWildcard = false;
    std::set<std::string> readPaths;
    bool readWildcard = false;
    std::set<std::string> writePaths;
    bool writeWildcard = false;

    void addFinding(const PermissionFinding& finding);
    std::string generateCliFlags() const;
    std::string generateReportText() const;
};

class PermissionScanner {
    friend struct PermissionReport;
public:
    static PermissionReport scan(const std::string& mainScriptPath);
    static std::string extractHostFromUrl(const std::string& url);

private:
    static void scanFile(const std::string& filePath, PermissionReport& report);
    static void scanStatements(const std::vector<StmtPtr>& statements, const std::string& currentFile, PermissionReport& report);
    static void scanStatement(StmtPtr stmt, const std::string& currentFile, PermissionReport& report);
    static void scanExpression(ExprPtr expr, const std::string& currentFile, PermissionReport& report);

    static std::string extractStringLiteral(ExprPtr expr);
    static std::string resolveModulePath(const std::string& currentFile, const std::string& importPath, std::string* outPkgDir = nullptr);
    static void checkPackageManifest(const std::string& dirPath, PermissionReport& report);
};

#endif // PERMISSION_SCANNER_H
