#include "Builtins.h"

void registerBuiltins(RuntimeContext& interp) {
    registerIOBuiltins(interp);
    registerNetBuiltins(interp);
    registerMathBuiltins(interp);
    registerStringBuiltins(interp);
    registerDataBuiltins(interp);
    registerSysBuiltins(interp);
    registerPDFBuiltins(interp);
    registerBufferBuiltins(interp);
    registerConcurrencyBuiltins(interp);
}
