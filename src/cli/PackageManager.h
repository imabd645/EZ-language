#ifndef PACKAGEMANAGER_H
#define PACKAGEMANAGER_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <algorithm>
#include <map>
#include <curl/curl.h>

namespace fs = std::filesystem;

// Minimal JSON implementation to replace jsoncpp dependency
#include "utils/MiniJson.h"

struct SemVer {
    int major = 0;
    int minor = 0;
    int patch = 0;
    std::string raw;

    SemVer() {}
    SemVer(const std::string& v) : raw(v) {
        std::string s = v;
        if (!s.empty() && s[0] == 'v') s = s.substr(1);
        int parsed = sscanf(s.c_str(), "%d.%d.%d", &major, &minor, &patch);
        if (parsed < 1) major = 0;
        if (parsed < 2) minor = 0;
        if (parsed < 3) patch = 0;
    }

    bool operator<(const SemVer& o) const {
        if (major != o.major) return major < o.major;
        if (minor != o.minor) return minor < o.minor;
        return patch < o.patch;
    }
    bool operator==(const SemVer& o) const {
        return major == o.major && minor == o.minor && patch == o.patch;
    }
    bool operator>=(const SemVer& o) const {
        return !(*this < o);
    }
};

struct VersionConstraint {
    enum Type { EXACT, CARET, GTE, ANY } type;
    SemVer target;

    VersionConstraint(const std::string& c) {
        if (c == "*" || c == "main" || c == "latest") {
            type = ANY;
        } else if (c[0] == '^') {
            type = CARET;
            target = SemVer(c.substr(1));
        } else if (c.size() > 1 && c[0] == '>' && c[1] == '=') {
            type = GTE;
            target = SemVer(c.substr(2));
        } else {
            type = EXACT;
            target = SemVer(c);
        }
    }

    bool satisfies(const SemVer& v) const {
        if (type == ANY) return true;
        if (type == EXACT) return v == target;
        if (type == GTE) return v >= target;
        if (type == CARET) {
            if (target.major > 0) return v.major == target.major && v >= target;
            if (target.minor > 0) return v.minor == target.minor && v >= target;
            return v.patch == target.patch;
        }
        return false;
    }
};

struct Package {
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    std::string mainFile;
    std::string repository;
    std::string localPath;
    std::vector<std::string> dependencies;
};

class PackageManager {
private:
    std::string packagesDir;
    std::string cacheDir;
    std::string configFile;
    std::unordered_map<std::string, Package> installedPackages;
    
    bool downloadFile(const std::string& url, const std::string& outputPath) {
        CURL* curl = curl_easy_init();
        if (!curl) return false;
        FILE* fp = fopen(outputPath.c_str(), "wb");
        if (!fp) {
            curl_easy_cleanup(curl);
            return false;
        }
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "cURL error: " << curl_easy_strerror(res) << std::endl;
        }
        fclose(fp);
        curl_easy_cleanup(curl);
        return res == CURLE_OK;
    }
    
    // Extract using system tar
    bool extractZip(const std::string& zipPath, const std::string& outputDir) {
        fs::create_directories(outputDir);
        std::string cmd = "tar -xf \"" + zipPath + "\" -C \"" + outputDir + "\" --strip-components=1 > nul 2>&1";
        return system(cmd.c_str()) == 0;
    }
    
    Package parsePackageFile(const std::string& filepath) {
        Package pkg;
        std::ifstream file(filepath);
        if (!file.is_open()) {
             pkg.version = "1.0.0";
             pkg.mainFile = "main.ez";
             return pkg;
        }
        
        MiniJson::Value root;
        MiniJson::Reader reader;
        if (!reader.parse(file, root)) return pkg;
        
        pkg.name = root.get("name", "").asString();
        pkg.version = root.get("version", "1.0.0").asString();
        pkg.description = root.get("description", "").asString();
        pkg.author = root.get("author", "").asString();
        pkg.mainFile = root.get("main", "main.ez").asString();
        pkg.repository = root.get("repository", "").asString();
        
        MiniJson::Value deps = root["dependencies"];
        if (!deps.isNull()) {
            for (const auto& name : deps.getMemberNames()) {
                pkg.dependencies.push_back(name + "@" + deps[name].asString());
            }
        }
        return pkg;
    }
    
    void saveConfig() {
        MiniJson::Value root(MiniJson::OBJECT);
        for (const auto& pair : installedPackages) {
            MiniJson::Value pkgJson(MiniJson::OBJECT);
            pkgJson["name"] = pair.second.name;
            pkgJson["version"] = pair.second.version;
            pkgJson["description"] = pair.second.description;
            pkgJson["author"] = pair.second.author;
            pkgJson["mainFile"] = pair.second.mainFile;
            pkgJson["repository"] = pair.second.repository;
            pkgJson["localPath"] = pair.second.localPath;
            
            MiniJson::Value deps(MiniJson::ARRAY);
            for (const auto& dep : pair.second.dependencies) deps.append(MiniJson::Value(dep));
            pkgJson["dependencies"] = deps;
            
            root[pair.first] = pkgJson;
        }
        
        MiniJson::StreamWriter writer;
        std::ofstream file(configFile);
        writer.write(root, &file);
    }
    
    void loadConfig() {
        installedPackages.clear();
        if (!fs::exists(configFile)) return;
        std::ifstream file(configFile);
        MiniJson::Value root;
        MiniJson::Reader reader;
        if (!reader.parse(file, root)) return;
        
        for (const auto& name : root.getMemberNames()) {
            MiniJson::Value pkgJson = root[name];
            Package pkg;
            pkg.name = pkgJson.get("name", "").asString();
            pkg.version = pkgJson.get("version", "1.0.0").asString();
            pkg.description = pkgJson.get("description", "").asString();
            pkg.author = pkgJson.get("author", "").asString();
            pkg.mainFile = pkgJson.get("mainFile", "main.ez").asString();
            pkg.repository = pkgJson.get("repository", "").asString();
            pkg.localPath = pkgJson.get("localPath", "").asString();
            
            MiniJson::Value deps = pkgJson["dependencies"];
            for (const auto& dep : deps.items) pkg.dependencies.push_back(dep.asString());
            // Note: Vector access via items
            
            installedPackages[name] = pkg;
        }
    }
    
    std::string getGitHubDownloadUrl(const std::string& repoUrl, const std::string& version = "main") {
        std::string downloadUrl = repoUrl;
        if (downloadUrl.find("github.com") != std::string::npos) {
             if (downloadUrl.back() == '/') downloadUrl.pop_back();
             downloadUrl += "/archive/refs/heads/" + version + ".zip";
        }
        return downloadUrl;
    }
    
    void installDependencies(const Package& pkg) {
        for (const std::string& dep : pkg.dependencies) {
            size_t atPos = dep.find('@');
            std::string depName = dep.substr(0, atPos);
            std::string depVersion = (atPos != std::string::npos) ? dep.substr(atPos + 1) : "main";
            if (!installedPackages.count(depName)) {
                installPackage(depName, depVersion);
            }
        }
    }

public:
    PackageManager(const std::string& baseDir = "C:/ezlib") {
        packagesDir = baseDir;
        cacheDir = baseDir + "/.cache";
        configFile = packagesDir + "/packages.json";
        
        fs::create_directories(packagesDir);
        fs::create_directories(cacheDir);
        loadConfig();
    }
    
    ~PackageManager() {
        fs::remove_all(cacheDir);
    }
    
    bool installPackage(const std::string& packageName, const std::string& versionConstraint = "latest", const std::string& repoUrl = "") {
        std::cout << "Installing " << packageName << "@" << versionConstraint << "..." << std::endl;
        
        if (installedPackages.count(packageName)) {
            Package& existing = installedPackages[packageName];
            VersionConstraint constraint(versionConstraint);
            if (constraint.satisfies(SemVer(existing.version))) {
                std::cout << "Already installed v" << existing.version << " which satisfies " << versionConstraint << std::endl;
                return true;
            }
        }
        
        std::string repositoryUrl = repoUrl;
        std::string downloadUrl = "";
        std::string targetVersion = "";
        std::string targetFolder = "ez" + packageName;
        
        if (repositoryUrl.empty()) {
            std::cout << "Fetching registry..." << std::endl;
            std::string indexUrl = "https://raw.githubusercontent.com/imabd645/ezlib/main/index.json";
            std::string indexPath = cacheDir + "/index.json";
            if (downloadFile(indexUrl, indexPath)) {
                std::ifstream file(indexPath);
                MiniJson::Value root;
                MiniJson::Reader reader;
                if (reader.parse(file, root) && root.properties.count("packages") > 0) {
                    bool pkgFound = false;
                    MiniJson::Value packages = root["packages"];
                    for (int i = 0; i < packages.items.size(); i++) {
                        MiniJson::Value pkg = packages.items[i];
                        if (pkg.get("name", "").asString() == packageName) {
                            pkgFound = true;
                            targetFolder = pkg.get("folder", targetFolder).asString();
                            VersionConstraint constraint(versionConstraint);
                            SemVer bestVer;
                            std::string bestUrl;
                            MiniJson::Value versions = pkg["versions"];
                            for (int j = 0; j < versions.items.size(); j++) {
                                MiniJson::Value vobj = versions.items[j];
                                SemVer sv(vobj.get("version", "").asString());
                                if (constraint.satisfies(sv) && sv >= bestVer) {
                                    bestVer = sv;
                                    bestUrl = vobj.get("url", "").asString();
                                    targetVersion = sv.raw;
                                }
                            }
                            if (!bestUrl.empty()) {
                                downloadUrl = bestUrl;
                            } else {
                                std::cerr << "Error: No version of '" << packageName << "' satisfies constraint '" << versionConstraint << "'." << std::endl;
                                return false;
                            }
                            break;
                        }
                    }
                    if (!pkgFound) {
                        std::cerr << "Error: Package '" << packageName << "' not found in the EZ registry." << std::endl;
                        return false;
                    }
                }
            }
        }
        
        if (targetVersion.empty()) targetVersion = (versionConstraint == "latest" || versionConstraint == "main") ? "main" : versionConstraint;
        if (downloadUrl.empty()) downloadUrl = getGitHubDownloadUrl(repositoryUrl.empty() ? "https://github.com/imabd645/ezlib" : repositoryUrl, targetVersion);
        
        std::string cachePath = cacheDir + "/ezlib-" + packageName + "-" + targetVersion + ".zip";
        
        std::cout << "Downloading from " << downloadUrl << "..." << std::endl;
        if (!downloadFile(downloadUrl, cachePath)) {
            std::cerr << "Download failed." << std::endl;
            return false;
        }
        
        std::string tempExtractDir = cacheDir + "/ezlib-temp";
        fs::remove_all(tempExtractDir);
        fs::create_directories(tempExtractDir);
        
        std::cout << "Extracting repository..." << std::endl;
        if (!extractZip(cachePath, tempExtractDir)) {
            std::cerr << "Extraction failed." << std::endl;
            return false;
        }
        
        std::string sourceLibDir = tempExtractDir + "/" + targetFolder;
        if (!fs::exists(sourceLibDir)) {
             sourceLibDir = tempExtractDir + "/ez" + packageName;
             if (!fs::exists(sourceLibDir)) {
                  sourceLibDir = tempExtractDir + "/" + packageName;
                  if (!fs::exists(sourceLibDir)) {
                      std::cerr << "Package '" << packageName << "' not found in the repository." << std::endl;
                      fs::remove_all(tempExtractDir);
                      return false;
                  }
             }
        }
        
        std::string extractDir = packagesDir + "/" + packageName;
        fs::remove_all(extractDir);
        
        try {
            fs::copy(sourceLibDir, extractDir, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        } catch (...) {
            std::cerr << "Failed to construct package directory." << std::endl;
            return false;
        }
        
        fs::remove_all(tempExtractDir);
        
        std::string packageEzPath = extractDir + "/package.ez";
        if (!fs::exists(packageEzPath)) {
             std::cout << "No package.ez found, creating default." << std::endl;
             Package def;
             def.name = packageName;
             def.version = targetVersion;
             def.localPath = extractDir;
             installedPackages[packageName] = def;
             saveConfig();
             return true;
        }
        
        Package pkg = parsePackageFile(packageEzPath);
        pkg.localPath = extractDir;
        if (pkg.name.empty()) pkg.name = packageName;
        
        installedPackages[packageName] = pkg;
        installDependencies(pkg);
        saveConfig();
        
        std::cout << "Installed " << packageName << " successfully." << std::endl;
        return true;
    }
    
    void listPackages() {
        std::cout << "Installed packages:" << std::endl;
        for (const auto& pair : installedPackages) {
            std::cout << " - " << pair.first << " (" << pair.second.version << ")" << std::endl;
        }
    }
    
    void initPackage(const std::string& name) {
        std::string dir = name;
        fs::create_directories(dir);
        std::ofstream f(dir + "/package.ez");
        f << "{\n  \"name\": \"" << name << "\",\n  \"version\": \"1.0.0\",\n  \"main\": \"main.ez\"\n}\n";
        f.close();
        
        std::ofstream m(dir + "/main.ez");
        m << "out \"Hello from " << name << "\"\n";
        m.close();
        std::cout << "Initialized package " << name << std::endl;
    }
};

#endif // PACKAGEMANAGER_H
