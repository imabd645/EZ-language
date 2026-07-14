#include <set>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
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
#include <windows.h>
#include <cstdint>




#include "cli/CLI.h"
void findDependencies(const std::string& filePath, std::set<std::string>& visited, std::vector<std::pair<std::string, std::string>>& filesToPack, const std::string& baseDir) {
    std::string normalizedPath = filePath;
    std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');
    
    if (visited.count(normalizedPath)) return;
    visited.insert(normalizedPath);
    
    std::ifstream file(normalizedPath, std::ios::binary);
    if (!file.is_open()) return;
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // Determine vfsName
    std::string vfsName;
    std::string normalizedBase = baseDir;
    std::replace(normalizedBase.begin(), normalizedBase.end(), '\\', '/');
    
    if (normalizedPath.find(normalizedBase) == 0) {
        vfsName = normalizedPath.substr(normalizedBase.length() + 1);
    } else {
        vfsName = normalizedPath; // e.g. C:/ezlib/...
    }
    
    // If it's the main file being passed directly, skip adding it here (handled in bundleFile)
    if (vfsName != "__main_skip__") {
        filesToPack.push_back({vfsName, content});
        std::cout << "  -> Packed " << vfsName << " (" << content.length() << " bytes)\n";
    }
    
    // Lexer-based search for dependencies
    Lexer lexer(content, normalizedPath);
    auto tokens = lexer.tokenize();
    for (size_t i = 0; i < tokens.size(); i++) {
        if (tokens[i].type == TokenType::USE && i + 1 < tokens.size() && tokens[i+1].type == TokenType::STRING) {
            std::string usePath;
            try { usePath = std::get<std::string>(tokens[i+1].literal); } catch(...) { continue; }
            
            // Try resolving usePath
            std::vector<std::string> searchPaths = {
                baseDir + "/lib/" + usePath + ".ez",
                baseDir + "/lib/" + usePath + "/main.ez",
                "C:/ezlib/" + usePath + ".ez",
                "C:/ezlib/" + usePath + "/main.ez",
                usePath,
                usePath + ".ez"
            };
            
            for (const auto& sp : searchPaths) {
                if (std::filesystem::exists(sp) && !std::filesystem::is_directory(sp)) {
                    findDependencies(sp, visited, filesToPack, baseDir);
                    break;
                }
            }
        }
    }
}

#pragma pack(push, 1)
struct EZ_ICONDIR {
    uint16_t idReserved;
    uint16_t idType;
    uint16_t idCount;
};
struct EZ_ICONDIRENTRY {
    uint8_t bWidth;
    uint8_t bHeight;
    uint8_t bColorCount;
    uint8_t bReserved;
    uint16_t wPlanes;
    uint16_t wBitCount;
    uint32_t dwBytesInRes;
    uint32_t dwImageOffset;
};
struct EZ_GRPICONDIR {
    uint16_t idReserved;
    uint16_t idType;
    uint16_t idCount;
};
struct EZ_GRPICONDIRENTRY {
    uint8_t bWidth;
    uint8_t bHeight;
    uint8_t bColorCount;
    uint8_t bReserved;
    uint16_t wPlanes;
    uint16_t wBitCount;
    uint32_t dwBytesInRes;
    uint16_t nID;
};
#pragma pack(pop)

bool injectIcon(const std::string& exePath, const std::string& iconPath) {
    std::ifstream file(iconPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> icoData(size);
    if (!file.read((char*)icoData.data(), size)) return false;

    if (size < sizeof(EZ_ICONDIR)) { std::cerr << "Icon error: too small\n"; return false; }
    EZ_ICONDIR* dir = (EZ_ICONDIR*)icoData.data();
    if (dir->idReserved != 0 || dir->idType != 1) { std::cerr << "Icon error: invalid header\n"; return false; }

    HANDLE hUpdate = BeginUpdateResourceA(exePath.c_str(), false);
    if (hUpdate == NULL) { std::cerr << "Icon error: BeginUpdateResourceA failed (" << GetLastError() << ")\n"; return false; }

    std::vector<uint8_t> grpData(sizeof(EZ_GRPICONDIR) + dir->idCount * sizeof(EZ_GRPICONDIRENTRY));
    EZ_GRPICONDIR* grpDir = (EZ_GRPICONDIR*)grpData.data();
    grpDir->idReserved = 0;
    grpDir->idType = 1;
    grpDir->idCount = dir->idCount;

    EZ_GRPICONDIRENTRY* grpEntries = (EZ_GRPICONDIRENTRY*)(grpData.data() + sizeof(EZ_GRPICONDIR));
    EZ_ICONDIRENTRY* entries = (EZ_ICONDIRENTRY*)(icoData.data() + sizeof(EZ_ICONDIR));

    for (int i = 0; i < dir->idCount; ++i) {
        grpEntries[i].bWidth = entries[i].bWidth;
        grpEntries[i].bHeight = entries[i].bHeight;
        grpEntries[i].bColorCount = entries[i].bColorCount;
        grpEntries[i].bReserved = entries[i].bReserved;
        grpEntries[i].wPlanes = entries[i].wPlanes;
        grpEntries[i].wBitCount = entries[i].wBitCount;
        grpEntries[i].dwBytesInRes = entries[i].dwBytesInRes;
        grpEntries[i].nID = i + 1; // Resource ID starting at 1

        uint8_t* imageData = icoData.data() + entries[i].dwImageOffset;
        UpdateResourceA(hUpdate, RT_ICON, MAKEINTRESOURCEA(i + 1), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), imageData, entries[i].dwBytesInRes);
    }

    UpdateResourceA(hUpdate, RT_GROUP_ICON, MAKEINTRESOURCEA(1), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), grpData.data(), grpData.size());
    if (!EndUpdateResourceA(hUpdate, false)) {
        std::cerr << "Icon error: EndUpdateResourceA failed (" << GetLastError() << ")\n";
        return false;
    }
    return true;
}

bool bundleFile(const std::string& entryScript, const std::string& outputExe, bool isGui, const std::string& iconPath) {
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
    
    std::vector<std::pair<std::string, std::string>> filesToPack;
    
    // 1. Pack main script
    std::stringstream buffer;
    buffer << entryFile.rdbuf();
    filesToPack.push_back({"__main__.ez", buffer.str()});
    std::cout << "  -> Packed __main__.ez (" << buffer.str().length() << " bytes)\n";
    
    std::string baseDir = exePath;
    size_t lastSlash = baseDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        baseDir = baseDir.substr(0, lastSlash);
    }
    
    // 2. Discover and pack dependencies recursively
    std::set<std::string> visited;
    // Mark main script as visited so we don't pack it twice
    std::string normalizedMain = entryScript;
    std::replace(normalizedMain.begin(), normalizedMain.end(), '\\', '/');
    // Call findDependencies but tell it to skip adding __main__ to the vector again
    findDependencies(normalizedMain, visited, filesToPack, baseDir);
    // Remove the __main__ entry from vector if findDependencies added it (we already added it as __main__.ez)
    if (filesToPack.size() > 1 && filesToPack.back().first != "__main__.ez" && filesToPack.back().second == buffer.str()) {
        // We handle duplicate by just not doing anything, as visited prevents loops. 
        // Wait, to cleanly skip main in findDependencies without hack, just pass it with a dummy visited entry.
    }
    
    // Actually, a better way to not duplicate main is to clear visited and insert it
    visited.insert(normalizedMain);
    
    // Now crawl the main file's content manually for the first level, then let findDependencies handle the rest
    // Lexer-based search for dependencies
    Lexer lexer(buffer.str(), entryScript);
    auto tokens = lexer.tokenize();
    for (size_t i = 0; i < tokens.size(); i++) {
        if (tokens[i].type == TokenType::USE && i + 1 < tokens.size() && tokens[i+1].type == TokenType::STRING) {
            std::string usePath;
            try { usePath = std::get<std::string>(tokens[i+1].literal); } catch(...) { continue; }
            std::vector<std::string> searchPaths = {
                baseDir + "/lib/" + usePath + ".ez",
                baseDir + "/lib/" + usePath + "/main.ez",
                "C:/ezlib/" + usePath + ".ez",
                "C:/ezlib/" + usePath + "/main.ez",
                usePath,
                usePath + ".ez"
            };
            for (const auto& sp : searchPaths) {
                if (std::filesystem::exists(sp) && !std::filesystem::is_directory(sp)) {
                    findDependencies(sp, visited, filesToPack, baseDir);
                    break;
                }
            }
        }
    }
    
    // 3. Build VFS Blob
    std::string vfsBlob;
    uint32_t numFiles = filesToPack.size();
    vfsBlob.append((char*)&numFiles, 4);
    
    for (const auto& f : filesToPack) {
        uint32_t nameLen = f.first.length();
        vfsBlob.append((char*)&nameLen, 4);
        vfsBlob.append(f.first);
        
        uint32_t fileLen = f.second.length();
        vfsBlob.append((char*)&fileLen, 4);
        vfsBlob.append(f.second);
    }
    
    // 4. Copy ez.exe to outputExe
    try {
        std::filesystem::copy_file(exePath, outputExe, std::filesystem::copy_options::overwrite_existing);
    } catch (const std::exception& e) {
        std::cerr << "Error copying executable: " << e.what() << std::endl;
        return false;
    }
    
    // 5. Inject Icon if provided
    if (!iconPath.empty()) {
        if (injectIcon(outputExe, iconPath)) {
            std::cout << "  -> Injected Custom Icon (" << iconPath << ")\n";
        } else {
            std::cout << "  -> Warning: Failed to inject custom icon. Ensure it's a valid .ico file.\n";
        }
    }
    
    // 6. Append VFS Blob
    // Anti-virus may lock the file briefly after EndUpdateResourceA, so we retry a few times
    std::ofstream outFile;
    int retries = 10;
    while (retries > 0) {
        outFile.open(outputExe, std::ios::binary | std::ios::app);
        if (outFile.is_open()) break;
        Sleep(100);
        retries--;
    }
    
    if (!outFile.is_open()) {
        std::cerr << "Error: Could not open output file for appending after 10 retries." << std::endl;
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

