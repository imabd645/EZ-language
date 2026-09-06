#include "cli/PermissionScanner.h"
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/ASTArena.h"
#include "utils/MiniJson.h"
#include "utils/EzLibPath.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

void PermissionReport::addFinding(const PermissionFinding& finding) {
    findings.push_back(finding);

    if (finding.type == "FFI") {
        needsFFI = true;
    } else if (finding.type == "PROCESS") {
        needsProcess = true;
    } else if (finding.type == "NET") {
        if (finding.target.empty() || finding.target == "<dynamic>") {
            netWildcard = true;
        } else {
            std::string host = PermissionScanner::extractHostFromUrl(finding.target);
            if (!host.empty()) netHosts.insert(host);
            else netWildcard = true;
        }
    } else if (finding.type == "READ") {
        if (finding.target.empty() || finding.target == "<dynamic>") {
            readWildcard = true;
        } else {
            readPaths.insert(finding.target);
        }
    } else if (finding.type == "WRITE") {
        if (finding.target.empty() || finding.target == "<dynamic>") {
            writeWildcard = true;
        } else {
            writePaths.insert(finding.target);
        }
    }
}

std::string PermissionReport::generateCliFlags() const {
    std::string flags = "--safe";
    if (needsFFI) flags += " --allow-ffi";
    if (needsProcess) flags += " --allow-process";

    if (netWildcard) {
        flags += " --allow-net";
    } else {
        for (const auto& host : netHosts) {
            flags += " --allow-net=" + host;
        }
    }

    if (readWildcard) {
        flags += " --allow-read";
    } else {
        for (const auto& p : readPaths) {
            flags += " --allow-read=" + p;
        }
    }

    if (writeWildcard) {
        flags += " --allow-write";
    } else {
        for (const auto& p : writePaths) {
            flags += " --allow-write=" + p;
        }
    }

    return flags;
}

std::string PermissionReport::generateReportText() const {
    std::ostringstream out;
    out << "=======================================================\n";
    out << " Permissions Analysis: " << targetScript << "\n";
    out << "=======================================================\n";
    out << " Scanned " << scannedFiles.size() << " file(s)";
    if (!packages.empty()) {
        out << ", " << packages.size() << " package manifest(s)";
    }
    out << ".\n\n";

    if (!packages.empty()) {
        out << "--- Imported Packages ---\n";
        for (const auto& pkg : packages) {
            out << "  * " << pkg.name << " (v" << (pkg.version.empty() ? "0.0.0" : pkg.version) << ")";
            if (!pkg.declaredPermissions.empty()) {
                out << " - Declared permissions: [";
                for (size_t i = 0; i < pkg.declaredPermissions.size(); i++) {
                    if (i > 0) out << ", ";
                    out << pkg.declaredPermissions[i];
                }
                out << "]";
            } else {
                out << " - (No permissions declared in package.ez)";
            }
            out << "\n";
        }
        out << "\n";
    }

    if (findings.empty()) {
        out << "No special capabilities detected.\n";
        out << "This script can safely run with:\n";
        out << "  ez " << targetScript << " --safe\n";
        return out.str();
    }

    out << "--- Detected Capabilities (" << findings.size() << ") ---\n";
    for (const auto& f : findings) {
        out << "  [" << f.type << "] " << f.operation;
        if (!f.target.empty()) {
            out << " (" << f.target << ")";
        }
        out << "\n      at " << f.file << ":" << f.line << "\n";
    }
    out << "\n";

    out << "--- Summary of Required Grants ---\n";
    if (needsFFI) out << "  --allow-ffi\n";
    if (needsProcess) out << "  --allow-process\n";
    if (netWildcard) {
        out << "  --allow-net (wildcard / dynamic URLs)\n";
    } else {
        for (const auto& h : netHosts) out << "  --allow-net=" << h << "\n";
    }
    if (readWildcard) {
        out << "  --allow-read (wildcard / dynamic paths)\n";
    } else {
        for (const auto& p : readPaths) out << "  --allow-read=" << p << "\n";
    }
    if (writeWildcard) {
        out << "  --allow-write (wildcard / dynamic paths)\n";
    } else {
        for (const auto& p : writePaths) out << "  --allow-write=" << p << "\n";
    }
    out << "\n";

    out << "--- Suggested Execution Command ---\n";
    out << "  ez " << targetScript << " " << generateCliFlags() << "\n";

    return out.str();
}

PermissionReport PermissionScanner::scan(const std::string& mainScriptPath) {
    PermissionReport report;
    report.targetScript = mainScriptPath;
    scanFile(mainScriptPath, report);
    return report;
}

void PermissionScanner::scanFile(const std::string& filePath, PermissionReport& report) {
    if (report.scannedFiles.count(filePath)) return;
    report.scannedFiles.insert(filePath);

    std::ifstream file(filePath);
    if (!file.is_open()) return;

    std::stringstream buf;
    buf << file.rdbuf();
    std::string source = buf.str();

    try {
        Lexer lexer(source, filePath);
        auto tokens = lexer.tokenize();
        ASTArena arena;
        Parser parser(tokens, arena);
        auto statements = parser.parse();
        scanStatements(statements, filePath, report);
    } catch (...) {
        // Continue scanning other files on parse error in one module
    }
}

void PermissionScanner::scanStatements(const std::vector<StmtPtr>& statements, const std::string& currentFile, PermissionReport& report) {
    for (const auto& stmt : statements) {
        scanStatement(stmt, currentFile, report);
    }
}

void PermissionScanner::scanStatement(StmtPtr stmt, const std::string& currentFile, PermissionReport& report) {
    if (!stmt) return;

    std::visit([&](auto* node) {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T, BlockStmt*>) {
            for (auto s : node->statements) scanStatement(s, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, ExpressionStmt*>) {
            scanExpression(node->expr, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, OutStmt*>) {
            scanExpression(node->expr, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, VarDeclStmt*>) {
            scanExpression(node->initializer, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, WhenStmt*>) {
            scanExpression(node->condition, currentFile, report);
            scanStatement(node->thenBranch, currentFile, report);
            scanStatement(node->elseBranch, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, WhileStmt*>) {
            scanExpression(node->condition, currentFile, report);
            scanStatement(node->body, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, RepeatStmt*>) {
            scanExpression(node->start, currentFile, report);
            scanExpression(node->end, currentFile, report);
            scanExpression(node->step, currentFile, report);
            scanStatement(node->body, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, GetStmt*>) {
            scanExpression(node->iterable, currentFile, report);
            scanStatement(node->body, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, MatchStmt*>) {
            scanExpression(node->subject, currentFile, report);
            for (const auto& arm : node->arms) {
                scanExpression(arm.pattern, currentFile, report);
                scanStatement(arm.body, currentFile, report);
            }
        }
        else if constexpr (std::is_same_v<T, TaskStmt*>) {
            for (auto def : node->defaultValues) scanExpression(def, currentFile, report);
            for (auto s : node->body) scanStatement(s, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, GiveStmt*>) {
            scanExpression(node->value, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, StaticStmt*>) {
            scanExpression(node->initializer, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, ModelStmt*>) {
            for (auto def : node->initDefaultValues) scanExpression(def, currentFile, report);
            for (auto s : node->initBody) scanStatement(s, currentFile, report);
            for (const auto& member : node->members) {
                scanExpression(member.initializer, currentFile, report);
                for (auto def : member.defaultValues) scanExpression(def, currentFile, report);
                for (auto s : member.body) scanStatement(s, currentFile, report);
            }
        }
        else if constexpr (std::is_same_v<T, StructStmt*>) {
            for (auto def : node->defaults) scanExpression(def, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, TryStmt*>) {
            scanStatement(node->tryBlock, currentFile, report);
            for (const auto& cb : node->catchBlocks) scanStatement(cb.body, currentFile, report);
            scanStatement(node->finallyBlock, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, ThrowStmt*>) {
            scanExpression(node->expr, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, ExportStmt*>) {
            scanStatement(node->inner, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, UseStmt*>) {
            std::string pkgDir;
            std::string resolved = resolveModulePath(currentFile, node->path, &pkgDir);
            if (!pkgDir.empty()) {
                checkPackageManifest(pkgDir, report);
            }
            if (!resolved.empty() && report.scannedFiles.count(resolved) == 0) {
                scanFile(resolved, report);
            }
        }
    }, stmt->variant);
}

void PermissionScanner::scanExpression(ExprPtr expr, const std::string& currentFile, PermissionReport& report) {
    if (!expr) return;

    std::visit([&](auto* node) {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T, BinaryExpr*>) {
            scanExpression(node->left, currentFile, report);
            scanExpression(node->right, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, UnaryExpr*>) {
            scanExpression(node->operand, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, CallExpr*>) {
            // Check callee function name
            std::string funcName;
            if (node->callee) {
                if (auto** idPtr = std::get_if<IdentifierExpr*>(&node->callee->variant)) {
                    funcName = (*idPtr)->name;
                } else if (auto** propPtr = std::get_if<PropertyAccessExpr*>(&node->callee->variant)) {
                    if (auto** objId = std::get_if<IdentifierExpr*>(&(*propPtr)->object->variant)) {
                        if ((*objId)->name == "FFI") {
                            PermissionFinding f;
                            f.type = "FFI";
                            f.operation = "FFI." + (*propPtr)->property;
                            f.target = (!node->arguments.empty()) ? extractStringLiteral(node->arguments[0]) : "";
                            f.file = currentFile;
                            f.line = expr->line;
                            f.column = expr->column;
                            report.addFinding(f);
                        }
                    }
                }
            }

            if (!funcName.empty()) {
                // 1. FFI functions
                if (funcName == "os_load_lib" || funcName == "os_find_func" || funcName == "os_call") {
                    PermissionFinding f;
                    f.type = "FFI";
                    f.operation = funcName;
                    f.target = (!node->arguments.empty()) ? extractStringLiteral(node->arguments[0]) : "";
                    f.file = currentFile;
                    f.line = expr->line;
                    f.column = expr->column;
                    report.addFinding(f);
                }
                // 2. Process functions
                else if (funcName == "system" || funcName == "exec") {
                    PermissionFinding f;
                    f.type = "PROCESS";
                    f.operation = funcName;
                    f.target = (!node->arguments.empty()) ? extractStringLiteral(node->arguments[0]) : "<dynamic>";
                    f.file = currentFile;
                    f.line = expr->line;
                    f.column = expr->column;
                    report.addFinding(f);
                }
                // 3. Network functions
                else if (funcName == "http_get" || funcName == "http_post" || funcName == "fetch" ||
                         funcName == "socket" || funcName == "tcp" || funcName == "udp") {
                    PermissionFinding f;
                    f.type = "NET";
                    f.operation = funcName;
                    f.target = (!node->arguments.empty()) ? extractStringLiteral(node->arguments[0]) : "<dynamic>";
                    f.file = currentFile;
                    f.line = expr->line;
                    f.column = expr->column;
                    report.addFinding(f);
                }
                // 4. File Read functions
                else if (funcName == "readFile" || funcName == "readLines" || funcName == "readBytes") {
                    PermissionFinding f;
                    f.type = "READ";
                    f.operation = funcName;
                    f.target = (!node->arguments.empty()) ? extractStringLiteral(node->arguments[0]) : "<dynamic>";
                    f.file = currentFile;
                    f.line = expr->line;
                    f.column = expr->column;
                    report.addFinding(f);
                }
                // 5. File Write functions
                else if (funcName == "writeFile" || funcName == "appendFile" || funcName == "writeLine" || funcName == "appendLine") {
                    PermissionFinding f;
                    f.type = "WRITE";
                    f.operation = funcName;
                    f.target = (!node->arguments.empty()) ? extractStringLiteral(node->arguments[0]) : "<dynamic>";
                    f.file = currentFile;
                    f.line = expr->line;
                    f.column = expr->column;
                    report.addFinding(f);
                }
                // 6. File constructor call (e.g. File("path", "w"))
                else if (funcName == "File") {
                    std::string pathStr = (!node->arguments.empty()) ? extractStringLiteral(node->arguments[0]) : "<dynamic>";
                    std::string modeStr = (node->arguments.size() >= 2) ? extractStringLiteral(node->arguments[1]) : "r";
                    
                    PermissionFinding f;
                    if (modeStr.find('w') != std::string::npos || modeStr.find('a') != std::string::npos || modeStr.find('+') != std::string::npos) {
                        f.type = "WRITE";
                    } else {
                        f.type = "READ";
                    }
                    f.operation = "File(" + modeStr + ")";
                    f.target = pathStr;
                    f.file = currentFile;
                    f.line = expr->line;
                    f.column = expr->column;
                    report.addFinding(f);
                }
            }

            scanExpression(node->callee, currentFile, report);
            for (auto arg : node->arguments) scanExpression(arg, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, IndexExpr*>) {
            scanExpression(node->object, currentFile, report);
            scanExpression(node->index, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, ArrayExpr*>) {
            for (auto el : node->elements) scanExpression(el, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, TupleExpr*>) {
            for (auto el : node->elements) scanExpression(el, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, AssignExpr*>) {
            scanExpression(node->value, currentFile, report);
            scanExpression(node->index, currentFile, report);
            scanExpression(node->object, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, LogicalExpr*>) {
            scanExpression(node->left, currentFile, report);
            scanExpression(node->right, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, LambdaExpr*>) {
            scanExpression(node->body, currentFile, report);
            for (auto s : node->stmtBody) scanStatement(s, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, PropertyAccessExpr*>) {
            scanExpression(node->object, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, NewExpr*>) {
            if (node->className == "File") {
                std::string pathStr = (!node->arguments.empty()) ? extractStringLiteral(node->arguments[0]) : "<dynamic>";
                std::string modeStr = (node->arguments.size() >= 2) ? extractStringLiteral(node->arguments[1]) : "r";
                
                PermissionFinding f;
                if (modeStr.find('w') != std::string::npos || modeStr.find('a') != std::string::npos || modeStr.find('+') != std::string::npos) {
                    f.type = "WRITE";
                } else {
                    f.type = "READ";
                }
                f.operation = "File(" + modeStr + ")";
                f.target = pathStr;
                f.file = currentFile;
                f.line = expr->line;
                f.column = expr->column;
                report.addFinding(f);
            }
            for (auto arg : node->arguments) scanExpression(arg, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, SetExpr*>) {
            scanExpression(node->object, currentFile, report);
            scanExpression(node->value, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, DictionaryExpr*>) {
            for (const auto& p : node->pairs) {
                scanExpression(p.first, currentFile, report);
                scanExpression(p.second, currentFile, report);
            }
        }
        else if constexpr (std::is_same_v<T, SpreadExpr*>) {
            scanExpression(node->expression, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, TernaryExpr*>) {
            scanExpression(node->condition, currentFile, report);
            scanExpression(node->thenBranch, currentFile, report);
            scanExpression(node->elseBranch, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, AwaitExpr*>) {
            scanExpression(node->expression, currentFile, report);
        }
        else if constexpr (std::is_same_v<T, DestructureAssignExpr*>) {
            for (auto target : node->targets) scanExpression(target, currentFile, report);
            scanExpression(node->value, currentFile, report);
        }
    }, expr->variant);
}

std::string PermissionScanner::extractStringLiteral(ExprPtr expr) {
    if (!expr) return "";
    if (auto** litPtr = std::get_if<LiteralExpr*>(&expr->variant)) {
        if (auto* s = std::get_if<std::string>(&(*litPtr)->value)) {
            return *s;
        }
    }
    return "<dynamic>";
}

std::string PermissionScanner::extractHostFromUrl(const std::string& url) {
    if (url.empty() || url == "<dynamic>") return "";

    std::string s = url;
    size_t protoPos = s.find("://");
    if (protoPos != std::string::npos) {
        s = s.substr(protoPos + 3);
    }

    size_t slashPos = s.find('/');
    if (slashPos != std::string::npos) {
        s = s.substr(0, slashPos);
    }

    size_t colonPos = s.find(':');
    if (colonPos != std::string::npos) {
        s = s.substr(0, colonPos);
    }

    return s;
}

std::string PermissionScanner::resolveModulePath(const std::string& currentFile, const std::string& importPath, std::string* outPkgDir) {
    fs::path curDir = fs::path(currentFile).parent_path();
    if (curDir.empty()) curDir = ".";

    std::vector<std::string> candidates = {
        (curDir / importPath).string(),
        (curDir / (importPath + ".ez")).string(),
        (curDir / importPath / "main.ez").string(),
        importPath,
        importPath + ".ez",
        (fs::path("lib") / importPath).string(),
        (fs::path("lib") / (importPath + ".ez")).string(),
        (fs::path("lib") / importPath / "main.ez").string(),
        (fs::path("lib") / importPath / (importPath + ".ez")).string(),
        (fs::path(ezLibBase()) / importPath).string(),
        (fs::path(ezLibBase()) / (importPath + ".ez")).string(),
        (fs::path(ezLibBase()) / importPath / "main.ez").string(),
        (fs::path(ezLibBase()) / importPath / (importPath + ".ez")).string()
    };

    for (const auto& c : candidates) {
        if (fs::exists(c) && !fs::is_directory(c)) {
            if (outPkgDir) {
                fs::path pDir = fs::path(c).parent_path();
                if (fs::exists(pDir / "package.ez")) {
                    *outPkgDir = pDir.string();
                }
            }
            return fs::weakly_canonical(c).string();
        }
    }

    return "";
}

void PermissionScanner::checkPackageManifest(const std::string& dirPath, PermissionReport& report) {
    fs::path manifestPath = fs::path(dirPath) / "package.ez";
    if (!fs::exists(manifestPath)) return;

    std::ifstream mf(manifestPath);
    if (!mf.is_open()) return;

    MiniJson::Reader reader;
    MiniJson::Value root;
    if (!reader.parse(mf, root)) return;

    ImportedPackageInfo info;
    info.name = root.get("name", "").asString();
    info.version = root.get("version", "").asString();
    info.path = dirPath;

    MiniJson::Value perms = root.get("permissions", MiniJson::Value(MiniJson::ARRAY));
    for (const auto& item : perms.items) {
        info.declaredPermissions.push_back(item.asString());
    }

    // Avoid duplicate package manifests in report
    for (const auto& existing : report.packages) {
        if (existing.name == info.name && existing.path == info.path) return;
    }

    report.packages.push_back(info);
}
