#ifndef BUILTINS_H
#define BUILTINS_H

class RuntimeContext;

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
