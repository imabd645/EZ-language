#include <cstdint>
#pragma once
#include <string>
int cli_main(int argc, char* argv[]);
void runRepl(bool traceExecution = false);
void runFile(const std::string& path, bool traceExecution = false);
bool patchPESubsystem(const std::string& exePath, uint16_t newSubsystem);

void compileFileToEzc(const std::string& path);
void dumpFileToEzasm(const std::string& path);
void showHelp();
void showVersion();
bool bundleFile(const std::string& entryScript, const std::string& outputExe, bool isGui, const std::string& iconPath);
