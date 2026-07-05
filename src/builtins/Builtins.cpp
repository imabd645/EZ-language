#include "builtins/Builtins.h"

void registerBuiltins(RuntimeContext& interp) {
    registerIOBuiltins(interp);
    registerNetBuiltins(interp);
    registerMathBuiltins(interp);
    registerStringBuiltins(interp);
    registerDataBuiltins(interp);
    registerCoreBuiltins(interp);
    registerGCBuiltins(interp);
    registerConsoleBuiltins(interp);
    registerFFIBuiltins(interp);
    registerBufferBuiltins(interp);
    registerConcurrencyBuiltins(interp);
    registerHttpBuiltins(interp);
}
