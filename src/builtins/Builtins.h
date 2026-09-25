#ifndef BUILTINS_H
#define BUILTINS_H

#include <string>
#include <vector>

class RuntimeContext;

extern std::vector<std::string> g_scriptArgs;
extern std::string g_scriptName;

void registerBuiltins(RuntimeContext& interp);

// data
void registerArrayBuiltins(RuntimeContext& interp);
void registerCSVBuiltins(RuntimeContext& interp);
void registerDictBuiltins(RuntimeContext& interp);
void registerFPBuiltins(RuntimeContext& interp);
void registerJSONBuiltins(RuntimeContext& interp);
void registerTypeBuiltins(RuntimeContext& interp);

// string
void registerStringBuiltins(RuntimeContext& interp);
void registerRegexBuiltins(RuntimeContext& interp);
void registerEncodingBuiltins(RuntimeContext& interp);

// io
void registerIOBuiltins(RuntimeContext& interp);
void registerFileBuiltins(RuntimeContext& interp);
void registerConsoleBuiltins(RuntimeContext& interp);
void ezReapDeadFileStreams();

// net
void registerNetBuiltins(RuntimeContext& interp);
void registerHttpBuiltins(RuntimeContext& interp);

// math
void registerMathBuiltins(RuntimeContext& interp);

// time
void registerTimerBuiltins(RuntimeContext& interp);
void registerDateTimeBuiltins(RuntimeContext& interp);
void registerTimeBuiltins(RuntimeContext& interp);

// core
void registerCoreBuiltins(RuntimeContext& interp);
void registerExceptionBuiltins(RuntimeContext& interp);
void registerGCBuiltins(RuntimeContext& interp);
void registerProcessBuiltins(RuntimeContext& interp);
void registerVMBuiltins(RuntimeContext& interp);
void registerConcurrencyBuiltins(RuntimeContext& interp);

// buffer
void registerBufferBuiltins(RuntimeContext& interp);

// ffi
void registerFFIBuiltins(RuntimeContext& interp);

void registerDBBuiltins(RuntimeContext& interp);

#endif // BUILTINS_H
