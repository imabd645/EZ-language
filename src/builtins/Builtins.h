#ifndef BUILTINS_H
#define BUILTINS_H

#include <string>
#include <vector>

class RuntimeContext;

// Command-line arguments intended for the running script, and the script's
// own path.
//
// The CLI fills these in before the program runs; they become the `argv` and
// `scriptName` globals. Kept here rather than read from main()'s argv inside
// a builtin because what counts as a script argument is a CLI decision --
// interpreter flags such as --trace are consumed there and must not reach the
// program.
//
// Empty when EZ is embedded or running a REPL, so `argv` is always a list and
// never nil.
extern std::vector<std::string> g_scriptArgs;
extern std::string g_scriptName;

void registerBuiltins(RuntimeContext& interp);

void registerIOBuiltins(RuntimeContext& interp);

// Release the OS handles of File objects that have been garbage collected.
// Defined in Builtins_IO.cpp, called by gc_collect() so an explicit collection
// also frees file descriptors, not just memory.
void ezReapDeadFileStreams();
void registerNetBuiltins(RuntimeContext& interp);
void registerDBBuiltins(RuntimeContext& interp);
void registerMathBuiltins(RuntimeContext& interp);
void registerStringBuiltins(RuntimeContext& interp);
void registerDataBuiltins(RuntimeContext& interp);
void registerCoreBuiltins(RuntimeContext& interp);
void registerGCBuiltins(RuntimeContext& interp);
void registerConsoleBuiltins(RuntimeContext& interp);
void registerFFIBuiltins(RuntimeContext& interp);
void registerBufferBuiltins(RuntimeContext& interp);
void registerConcurrencyBuiltins(RuntimeContext& interp);
void registerHttpBuiltins(RuntimeContext& interp);
void registerTimeDateBuiltins(RuntimeContext& interp);

#endif // BUILTINS_H
