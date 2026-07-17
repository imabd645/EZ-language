#ifndef BUILTINS_H
#define BUILTINS_H

class RuntimeContext;

void registerBuiltins(RuntimeContext& interp);

void registerIOBuiltins(RuntimeContext& interp);
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
