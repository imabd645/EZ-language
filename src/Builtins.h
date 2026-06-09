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
void registerSysBuiltins(RuntimeContext& interp);
void registerBufferBuiltins(RuntimeContext& interp);
void registerConcurrencyBuiltins(RuntimeContext& interp);

#endif // BUILTINS_H
