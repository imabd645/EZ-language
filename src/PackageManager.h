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

namespace fs = std::filesystem;

// Minimal JSON implementation to replace jsoncpp dependency
#include "MiniJson.h"

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
        std::string cmd = "curl -L -o \"" + outputPath + "\" \"" + url + "\" > nul 2>&1";
        return system(cmd.c_str()) == 0;
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
    
    bool installPackage(const std::string& packageName, const std::string& version = "main", const std::string& repoUrl = "") {
        std::cout << "Installing " << packageName << "@" << version << "..." << std::endl;
        
        if (installedPackages.count(packageName)) {
            Package& existing = installedPackages[packageName];
            std::cout << "Already installed v" << existing.version << std::endl;
            if (existing.version == version) return true;
        }
        
        std::string repositoryUrl = repoUrl;
        if (repositoryUrl.empty()) {
            repositoryUrl = "https://github.com/imabd645/EZ" + packageName;
        }
        
        std::string downloadUrl = getGitHubDownloadUrl(repositoryUrl, version);
        std::string cachePath = cacheDir + "/" + packageName + "-" + version + ".zip";
        
        std::cout << "Downloading from " << downloadUrl << "..." << std::endl;
        if (!downloadFile(downloadUrl, cachePath)) {
            std::cerr << "Download failed." << std::endl;
            return false;
        }
        
        std::string extractDir = packagesDir + "/" + packageName;
        fs::remove_all(extractDir);
        fs::create_directories(extractDir);
        
        std::cout << "Extracting..." << std::endl;
        if (!extractZip(cachePath, extractDir)) {
            std::cerr << "Extraction failed." << std::endl;
            return false;
        }
        
        std::string packageEzPath = extractDir + "/package.ez";
        if (!fs::exists(packageEzPath)) {
             std::cout << "No package.ez found, creating default." << std::endl;
             Package def;
             def.name = packageName;
             def.version = version;
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
