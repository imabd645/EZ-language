#ifndef PACKAGEMANAGER_H
#define PACKAGEMANAGER_H

// ============================================================================
// Package management against an ez-registry instance.
//
// Dependency resolution happens on the SERVER: the client posts the set of
// requirements it wants and receives a flat, ordered install plan with a
// tarball URL and a SHA-256 for each entry. That keeps a single, testable
// implementation of version solving in one place instead of one per client,
// and means an old binary never resolves a graph with stale rules.
//
// This side therefore only has to: ask, download, verify, unpack, record.
// ============================================================================

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <filesystem>
#include <cstdlib>

#include "cli/Registry.h"
#include "utils/MiniJson.h"

namespace fs = std::filesystem;

struct PlannedPackage {
    std::string name;
    std::string version;
    std::string installDir;
    std::string mainFile;
    std::string tarball;
    std::string shasum;
    long long   size = 0;
    bool        direct = false;
};

class PackageManager {
public:
    explicit PackageManager(const std::string& projectRoot = ".")
        : root(projectRoot), registry(ezreg::registryUrl()) {}

    const std::string& registryUrl() const { return registry; }

    // ── install ─────────────────────────────────────────────────────────────

    /**
     * Install everything listed in package.ez.
     */
    bool installAll() {
        std::map<std::string, std::string> deps;
        if (!readManifestDependencies(deps)) return false;
        if (deps.empty()) {
            std::cout << "No dependencies listed in package.ez." << std::endl;
            return true;
        }
        return installResolved(deps, {});
    }

    /**
     * Install one package (optionally `name@range`) and record it in
     * package.ez so the next `ez install` reproduces this state.
     */
    bool installOne(const std::string& spec) {
        std::string name = spec;
        std::string range = "*";
        size_t at = spec.find('@', 1);   // from 1: a leading @ is not a separator
        if (at != std::string::npos) {
            name = spec.substr(0, at);
            range = spec.substr(at + 1);
            if (range.empty()) range = "*";
        }

        std::map<std::string, std::string> deps;
        readManifestDependencies(deps);          // existing ones stay pinned
        deps[name] = range;

        std::set<std::string> added{name};
        if (!installResolved(deps, added)) return false;
        return true;
    }

    bool uninstall(const std::string& name) {
        std::map<std::string, std::string> deps;
        readManifestDependencies(deps);
        bool listed = deps.erase(name) > 0;

        fs::path dir = fs::path(root) / "lib" / name;
        std::error_code ec;
        bool removed = fs::exists(dir) && fs::remove_all(dir, ec) > 0;

        if (!listed && !removed) {
            std::cerr << "'" << name << "' is not installed." << std::endl;
            return false;
        }
        if (listed) writeManifestDependencies(deps);
        std::cout << "Removed " << name << std::endl;
        return true;
    }

    // ── query ───────────────────────────────────────────────────────────────

    bool search(const std::string& query) {
        ezreg::Response r = ezreg::Http::get(registry + "/api/v1/packages?q=" + urlEncode(query));
        if (!r.ok()) {
            std::cerr << "Search failed: " << ezreg::errorMessage(r) << std::endl;
            return false;
        }
        bool ok = false;
        MiniJson::Value body = ezreg::parseJson(r.body, ok);
        if (!ok) { std::cerr << "Registry returned malformed JSON." << std::endl; return false; }

        const auto& list = body["packages"].items;
        if (list.empty()) {
            std::cout << "No packages match '" << query << "'." << std::endl;
            return true;
        }
        for (const auto& p : list) {
            std::cout << "  " << pad(p.get("name", "").asString(), 22)
                      << pad(p.get("latest", "").asString(), 12)
                      << p.get("description", "").asString() << std::endl;
        }
        std::cout << "\n" << list.size() << " of " << body.get("total", "0").asString()
                  << " package(s)." << std::endl;
        return true;
    }

    bool info(const std::string& name) {
        ezreg::Response r = ezreg::Http::get(registry + "/api/v1/packages/" + urlEncode(name));
        if (!r.ok()) {
            std::cerr << "Cannot fetch '" << name << "': " << ezreg::errorMessage(r) << std::endl;
            return false;
        }
        bool ok = false;
        MiniJson::Value p = ezreg::parseJson(r.body, ok);
        if (!ok) { std::cerr << "Registry returned malformed JSON." << std::endl; return false; }

        std::cout << p.get("name", "").asString() << "  " << p.get("latest", "").asString() << "\n"
                  << p.get("description", "").asString() << "\n\n";
        std::string license = p.get("license", "").asString();
        std::string repo = p.get("repository", "").asString();
        if (!license.empty()) std::cout << "  license      " << license << "\n";
        if (!repo.empty())    std::cout << "  repository   " << repo << "\n";
        std::cout << "  downloads    " << p.get("downloads", "0").asString() << "\n";

        std::cout << "  owners       ";
        for (size_t i = 0; i < p["owners"].items.size(); ++i) {
            std::cout << (i ? ", " : "") << p["owners"].items[i].asString();
        }
        std::cout << "\n  versions     ";
        const auto& versions = p["versions"].items;
        for (size_t i = 0; i < versions.size() && i < 10; ++i) {
            std::cout << (i ? ", " : "") << versions[i].get("version", "").asString();
            if (versions[i].get("yanked", "false").asString() == "true") std::cout << " (yanked)";
        }
        if (versions.size() > 10) std::cout << ", ... (" << versions.size() << " total)";
        std::cout << std::endl;

        if (!versions.empty()) {
            const auto& deps = versions[0]["dependencies"];
            if (!deps.getMemberNames().empty()) {
                std::cout << "\n  dependencies of " << versions[0].get("version", "").asString() << ":\n";
                for (const auto& key : deps.getMemberNames()) {
                    std::cout << "    " << pad(key, 20) << deps[key].asString() << "\n";
                }
            }
        }
        return true;
    }

    void listInstalled() {
        fs::path libDir = fs::path(root) / "lib";
        if (!fs::exists(libDir)) {
            std::cout << "No packages installed." << std::endl;
            return;
        }
        std::map<std::string, std::string> locked;
        readLock(locked);

        int count = 0;
        for (const auto& entry : fs::directory_iterator(libDir)) {
            if (!entry.is_directory()) continue;
            std::string name = entry.path().filename().string();
            auto it = locked.find(name);
            std::cout << "  " << pad(name, 24) << (it == locked.end() ? "(unmanaged)" : it->second)
                      << std::endl;
            count++;
        }
        if (count == 0) std::cout << "No packages installed." << std::endl;
    }

    // ── accounts ────────────────────────────────────────────────────────────

    bool login(const std::string& username, const std::string& password) {
        std::string payload = "{\"username\":\"" + ezreg::jsonEscape(username) +
                              "\",\"password\":\"" + ezreg::jsonEscape(password) +
                              "\",\"tokenName\":\"ez-cli\"}";
        ezreg::Response r = ezreg::Http::postJson(registry + "/api/v1/login", payload);
        if (!r.ok()) {
            std::cerr << "Login failed: " << ezreg::errorMessage(r) << std::endl;
            return false;
        }
        bool ok = false;
        MiniJson::Value body = ezreg::parseJson(r.body, ok);
        if (!ok) { std::cerr << "Registry returned malformed JSON." << std::endl; return false; }
        std::string token = body.get("token", "").asString();
        if (token.empty()) { std::cerr << "Registry did not return a token." << std::endl; return false; }

        ezreg::storeToken(registry, token, username);
        std::cout << "Logged in to " << registry << " as " << username << std::endl;
        return true;
    }

    bool registerAccount(const std::string& username, const std::string& email,
                         const std::string& password) {
        std::string payload = "{\"username\":\"" + ezreg::jsonEscape(username) +
                              "\",\"email\":\"" + ezreg::jsonEscape(email) +
                              "\",\"password\":\"" + ezreg::jsonEscape(password) + "\"}";
        ezreg::Response r = ezreg::Http::postJson(registry + "/api/v1/register", payload);
        if (!r.ok()) {
            std::cerr << "Registration failed: " << ezreg::errorMessage(r) << std::endl;
            return false;
        }
        bool ok = false;
        MiniJson::Value body = ezreg::parseJson(r.body, ok);
        std::string token = ok ? body.get("token", "").asString() : "";
        if (!token.empty()) ezreg::storeToken(registry, token, username);
        std::cout << "Account created; logged in as " << username << std::endl;
        return true;
    }

    bool logout() {
        ezreg::storeToken(registry, "", "");
        std::cout << "Logged out of " << registry << std::endl;
        return true;
    }

    bool whoami() {
        std::string token = ezreg::storedToken(registry);
        if (token.empty()) {
            std::cout << "Not logged in to " << registry << ". Run: ez login" << std::endl;
            return false;
        }
        ezreg::Response r = ezreg::Http::get(registry + "/api/v1/me", token);
        if (!r.ok()) {
            std::cerr << "Not authenticated: " << ezreg::errorMessage(r) << std::endl;
            return false;
        }
        bool ok = false;
        MiniJson::Value body = ezreg::parseJson(r.body, ok);
        std::cout << body.get("username", "?").asString() << " <" << body.get("email", "").asString()
                  << "> on " << registry << std::endl;
        return true;
    }

    // ── publish ─────────────────────────────────────────────────────────────

    bool publish() {
        std::string manifestPath = (fs::path(root) / "package.ez").string();
        std::string manifest = ezreg::readFileText(manifestPath);
        if (manifest.empty()) {
            std::cerr << "No package.ez found in " << fs::absolute(root).string() << std::endl;
            std::cerr << "Run 'ez init <name>' to create one." << std::endl;
            return false;
        }
        bool ok = false;
        MiniJson::Value m = ezreg::parseJson(manifest, ok);
        if (!ok) { std::cerr << "package.ez is not valid JSON." << std::endl; return false; }

        std::string name = m.get("name", "").asString();
        std::string version = m.get("version", "").asString();
        if (name.empty() || version.empty()) {
            std::cerr << "package.ez must set both \"name\" and \"version\"." << std::endl;
            return false;
        }

        std::string token = ezreg::storedToken(registry);
        if (token.empty()) {
            std::cerr << "Not logged in to " << registry << ". Run: ez login" << std::endl;
            return false;
        }

        std::string tarball = (fs::path(ezreg::cacheDir()) / (name + "-" + version + ".tgz")).string();
        if (!createTarball(tarball)) return false;

        std::string readme = ezreg::readFileText((fs::path(root) / "README.md").string());

        std::cout << "Publishing " << name << "@" << version << " to " << registry << " ..." << std::endl;
        ezreg::Response r = ezreg::Http::publish(registry + "/api/v1/publish", manifest,
                                                 tarball, readme, token);
        std::error_code ec;
        fs::remove(tarball, ec);

        if (!r.ok()) {
            std::cerr << "Publish failed: " << ezreg::errorMessage(r) << std::endl;
            return false;
        }
        MiniJson::Value body = ezreg::parseJson(r.body, ok);
        std::cout << "Published " << name << "@" << version << "\n"
                  << "  sha256 " << body.get("shasum", "").asString() << std::endl;
        return true;
    }

    bool yank(const std::string& spec, bool yanked) {
        size_t at = spec.find('@', 1);
        if (at == std::string::npos) {
            std::cerr << "Usage: ez " << (yanked ? "yank" : "unyank") << " <package>@<version>" << std::endl;
            return false;
        }
        std::string name = spec.substr(0, at);
        std::string version = spec.substr(at + 1);
        std::string token = ezreg::storedToken(registry);
        if (token.empty()) {
            std::cerr << "Not logged in. Run: ez login" << std::endl;
            return false;
        }
        std::string payload = std::string("{\"yanked\":") + (yanked ? "true" : "false") + "}";
        ezreg::Response r = ezreg::Http::postJson(
            registry + "/api/v1/packages/" + urlEncode(name) + "/" + urlEncode(version) + "/yank",
            payload, token);
        if (!r.ok()) {
            std::cerr << "Failed: " << ezreg::errorMessage(r) << std::endl;
            return false;
        }
        std::cout << (yanked ? "Yanked " : "Unyanked ") << name << "@" << version << std::endl;
        return true;
    }

    // ── scaffolding ─────────────────────────────────────────────────────────

    bool initPackage(const std::string& name) {
        fs::path dir = fs::path(root) / name;
        std::error_code ec;
        if (fs::exists(dir)) {
            std::cerr << "'" << name << "' already exists." << std::endl;
            return false;
        }
        fs::create_directories(dir, ec);
        ezreg::writeFileText((dir / "package.ez").string(),
            "{\n"
            "  \"name\": \"" + ezreg::jsonEscape(name) + "\",\n"
            "  \"version\": \"0.1.0\",\n"
            "  \"description\": \"\",\n"
            "  \"main\": \"main.ez\",\n"
            "  \"license\": \"MIT\",\n"
            "  \"keywords\": [],\n"
            "  \"dependencies\": {}\n"
            "}\n");
        ezreg::writeFileText((dir / "main.ez").string(),
            "# " + name + "\n\nexport task hello() {\n    give \"hello from " + name + "\"\n}\n");
        ezreg::writeFileText((dir / "README.md").string(), "# " + name + "\n");
        std::cout << "Created " << name << "/ (package.ez, main.ez, README.md)" << std::endl;
        return true;
    }

private:
    std::string root;
    std::string registry;

    static std::string pad(const std::string& s, size_t width) {
        std::string out = s;
        while (out.size() < width) out += ' ';
        return out;
    }

    static std::string urlEncode(const std::string& s) {
        static const char* hex = "0123456789ABCDEF";
        std::string out;
        for (unsigned char c : s) {
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                out += static_cast<char>(c);
            } else {
                out += '%';
                out += hex[c >> 4];
                out += hex[c & 0xF];
            }
        }
        return out;
    }

    std::string manifestPath() const { return (fs::path(root) / "package.ez").string(); }
    std::string lockPath() const { return (fs::path(root) / "ez.lock").string(); }

    bool readManifestDependencies(std::map<std::string, std::string>& out) {
        std::string text = ezreg::readFileText(manifestPath());
        if (text.empty()) {
            // No manifest is fine for `ez install <pkg>`; installAll() reports it.
            return true;
        }
        bool ok = false;
        MiniJson::Value m = ezreg::parseJson(text, ok);
        if (!ok) {
            std::cerr << "package.ez is not valid JSON." << std::endl;
            return false;
        }
        if (m.properties.count("dependencies")) {
            MiniJson::Value deps = m["dependencies"];
            for (const auto& key : deps.getMemberNames()) out[key] = deps[key].asString();
        }
        return true;
    }

    /**
     * Rewrite only the dependencies block, leaving the rest of package.ez as
     * the author wrote it -- comments and key order included. Re-serialising
     * the whole document would quietly reformat a file the user maintains.
     */
    bool writeManifestDependencies(const std::map<std::string, std::string>& deps) {
        std::string text = ezreg::readFileText(manifestPath());
        std::string block = "\"dependencies\": {";
        bool first = true;
        for (const auto& [name, range] : deps) {
            block += (first ? "\n" : ",\n");
            block += "    \"" + ezreg::jsonEscape(name) + "\": \"" + ezreg::jsonEscape(range) + "\"";
            first = false;
        }
        block += first ? "}" : "\n  }";

        if (text.empty()) {
            return ezreg::writeFileText(manifestPath(), "{\n  " + block + "\n}\n");
        }

        size_t start = text.find("\"dependencies\"");
        if (start == std::string::npos) {
            size_t lastBrace = text.rfind('}');
            if (lastBrace == std::string::npos) return false;
            size_t before = text.find_last_not_of(" \t\r\n", lastBrace - 1);
            std::string sep = (before != std::string::npos && text[before] == '{') ? "\n  " : ",\n  ";
            text.insert(lastBrace, sep + block + "\n");
            return ezreg::writeFileText(manifestPath(), text);
        }

        size_t open = text.find('{', start);
        if (open == std::string::npos) return false;
        int depth = 0;
        size_t close = open;
        for (size_t i = open; i < text.size(); ++i) {
            if (text[i] == '{') depth++;
            else if (text[i] == '}') { depth--; if (depth == 0) { close = i; break; } }
        }
        text.replace(start, close - start + 1, block);
        return ezreg::writeFileText(manifestPath(), text);
    }

    void readLock(std::map<std::string, std::string>& out) {
        bool ok = false;
        MiniJson::Value lock = ezreg::parseJson(ezreg::readFileText(lockPath()), ok);
        if (!ok || !lock.properties.count("packages")) return;
        MiniJson::Value pkgs = lock["packages"];
        for (const auto& key : pkgs.getMemberNames()) {
            out[key] = pkgs[key].get("version", "").asString();
        }
    }

    /**
     * ez.lock records exactly what was installed, with checksums, so a later
     * install can be reproduced and verified rather than re-resolved to
     * whatever is newest today.
     */
    void writeLock(const std::vector<PlannedPackage>& plan) {
        std::string out = "{\n  \"registry\": \"" + ezreg::jsonEscape(registry) + "\",\n  \"packages\": {";
        bool first = true;
        for (const auto& p : plan) {
            out += (first ? "\n" : ",\n");
            out += "    \"" + ezreg::jsonEscape(p.name) + "\": {\n"
                   "      \"version\": \"" + ezreg::jsonEscape(p.version) + "\",\n"
                   "      \"sha256\": \"" + ezreg::jsonEscape(p.shasum) + "\",\n"
                   "      \"direct\": " + (p.direct ? "true" : "false") + "\n    }";
            first = false;
        }
        out += first ? "}\n}\n" : "\n  }\n}\n";
        ezreg::writeFileText(lockPath(), out);
    }

    /** Ask the registry for a full install plan. */
    bool resolvePlan(const std::map<std::string, std::string>& deps,
                     std::vector<PlannedPackage>& plan) {
        std::string payload = "{\"dependencies\":{";
        bool first = true;
        for (const auto& [name, range] : deps) {
            if (!first) payload += ",";
            payload += "\"" + ezreg::jsonEscape(name) + "\":\"" + ezreg::jsonEscape(range) + "\"";
            first = false;
        }
        payload += "}}";

        ezreg::Response r = ezreg::Http::postJson(registry + "/api/v1/resolve", payload);
        if (!r.ok()) {
            std::cerr << "Could not resolve dependencies:\n  " << ezreg::errorMessage(r) << std::endl;
            return false;
        }
        bool ok = false;
        MiniJson::Value body = ezreg::parseJson(r.body, ok);
        if (!ok || !body.properties.count("install")) {
            std::cerr << "Registry returned an unexpected response." << std::endl;
            return false;
        }
        for (const auto& item : body["install"].items) {
            PlannedPackage p;
            p.name       = item.get("name", "").asString();
            p.version    = item.get("version", "").asString();
            p.installDir = item.get("installDir", "").asString();
            if (p.installDir.empty()) p.installDir = p.name;
            p.mainFile   = item.get("main", "main.ez").asString();
            p.tarball    = item.get("tarball", "").asString();
            p.shasum     = item.get("shasum", "").asString();
            p.direct     = item.get("direct", "false").asString() == "true";
            plan.push_back(p);
        }
        return true;
    }

    bool installResolved(const std::map<std::string, std::string>& deps,
                         const std::set<std::string>& newlyAdded) {
        std::cout << "Resolving from " << registry << " ..." << std::endl;
        std::vector<PlannedPackage> plan;
        if (!resolvePlan(deps, plan)) return false;
        if (plan.empty()) {
            std::cout << "Nothing to install." << std::endl;
            return true;
        }

        std::cout << "Installing " << plan.size() << " package"
                  << (plan.size() == 1 ? "" : "s") << ":" << std::endl;
        for (const auto& p : plan) {
            std::cout << "  " << pad(p.name, 22) << p.version
                      << (p.direct ? "" : "  (dependency)") << std::endl;
        }

        // The plan arrives dependency-first, so installing straight down the
        // list never leaves a package without something it needs.
        for (const auto& p : plan) {
            if (!installPlanned(p)) return false;
        }

        std::map<std::string, std::string> updated = deps;
        for (const auto& name : newlyAdded) {
            // Record what was actually chosen, as a caret range: reproducible
            // without pinning the user out of compatible updates.
            for (const auto& p : plan) {
                if (p.name == name) updated[name] = "^" + p.version;
            }
        }
        if (!newlyAdded.empty()) writeManifestDependencies(updated);
        writeLock(plan);

        std::cout << "Done." << std::endl;
        return true;
    }

    bool installPlanned(const PlannedPackage& p) {
        std::string cached = (fs::path(ezreg::cacheDir()) /
                              (p.name + "-" + p.version + ".tgz")).string();

        // A cached tarball is only reused if its checksum still matches; a
        // truncated or tampered cache entry must not be trusted.
        bool haveValid = fs::exists(cached) && !p.shasum.empty() &&
                         ezreg::sha256File(cached) == p.shasum;

        if (!haveValid) {
            std::string err;
            if (!ezreg::Http::download(p.tarball, cached, err)) {
                std::cerr << "  Download of " << p.name << "@" << p.version << " failed: "
                          << err << std::endl;
                return false;
            }
            std::string actual = ezreg::sha256File(cached);
            if (!p.shasum.empty() && actual != p.shasum) {
                std::error_code ec;
                fs::remove(cached, ec);
                std::cerr << "  Checksum mismatch for " << p.name << "@" << p.version << "\n"
                          << "    expected " << p.shasum << "\n"
                          << "    got      " << actual << "\n"
                          << "  Refusing to install." << std::endl;
                return false;
            }
        }

        fs::path target = fs::path(root) / "lib" / p.installDir;
        std::error_code ec;
        fs::remove_all(target, ec);
        fs::create_directories(target, ec);
        if (ec) {
            std::cerr << "  Cannot create " << target.string() << std::endl;
            return false;
        }

        if (!extractTarball(cached, target.string())) {
            std::cerr << "  Could not unpack " << p.name << "@" << p.version << std::endl;
            return false;
        }
        return true;
    }

    /**
     * Unpack with the system tar. Windows 10+ ships bsdtar as tar.exe, so this
     * needs nothing installed on any supported platform.
     *
     * --strip-components=1 is deliberately NOT used: the archive is created by
     * `ez publish` with its files at the root, so stripping would discard them.
     */
    bool extractTarball(const std::string& archive, const std::string& outDir) {
        std::string cmd = "tar -xzf \"" + archive + "\" -C \"" + outDir + "\"";
#ifdef _WIN32
        cmd += " 2>nul";
#else
        cmd += " 2>/dev/null";
#endif
        return std::system(cmd.c_str()) == 0;
    }

    /** Build the publishable archive from the project directory. */
    bool createTarball(const std::string& outPath) {
        std::error_code ec;
        fs::remove(outPath, ec);

        // Excludes keep the archive to source: no VCS metadata, no installed
        // dependencies (they are resolved fresh), no local caches.
        std::string cmd = "tar -czf \"" + outPath + "\""
                          " --exclude=.git --exclude=node_modules --exclude=lib"
                          " --exclude=ez.lock --exclude=data --exclude=\"*.tgz\""
                          " -C \"" + fs::absolute(root).string() + "\" .";
#ifdef _WIN32
        cmd += " 2>nul";
#else
        cmd += " 2>/dev/null";
#endif
        if (std::system(cmd.c_str()) != 0 || !fs::exists(outPath)) {
            std::cerr << "Could not create the package archive (is `tar` available?)." << std::endl;
            return false;
        }
        return true;
    }
};

#endif
