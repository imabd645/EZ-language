#ifndef BUILTINS_H
#define BUILTINS_H

#include <string>
#include <vector>

class RuntimeContext;

extern std::vector<std::string> g_scriptArgs;
extern std::string g_scriptName;

void registerBuiltins(RuntimeContext& interp);

void registerIOBuiltins(RuntimeContext& interp);
void ezReapDeadFileStreams();
void registerNetBuiltins(RuntimeContext& interp);
void registerDBBuiltins(RuntimeContext& interp);
void registerMathBuiltins(RuntimeContext& interp);
void registerStringBuiltins(RuntimeContext& interp);
void registerDataBuiltins(RuntimeContext& interp);
void registerCoreBuiltins(RuntimeContext& interp);
void registerConsoleBuiltins(RuntimeContext& interp);
void registerFFIBuiltins(RuntimeContext& interp);
void registerBufferBuiltins(RuntimeContext& interp);
void registerHttpBuiltins(RuntimeContext& interp);
void registerTimeDateBuiltins(RuntimeContext& interp);

// Modularized builtins
void registerGCBuiltins(RuntimeContext& interp);
void registerVMBuiltins(RuntimeContext& interp);
void registerProcessBuiltins(RuntimeContext& interp);
void registerExceptionBuiltins(RuntimeContext& interp);
void registerTimeBuiltins(RuntimeContext& interp);
void registerConcurrencyBuiltins(RuntimeContext& interp);

#endif // BUILTINS_H
