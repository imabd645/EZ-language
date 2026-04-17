#include "Builtins.h"

void registerBuiltins(Interpreter& interp) {
    registerIOBuiltins(interp);
    registerNetBuiltins(interp);
    registerNetBuiltins(interp);
    // registerDBBuiltins(interp); // Moved to lib/db.ez via FFI
    registerMathBuiltins(interp);
    registerMathBuiltins(interp);
    registerStringBuiltins(interp);
    registerDataBuiltins(interp);
    registerSysBuiltins(interp);
    registerPDFBuiltins(interp);
    registerBufferBuiltins(interp);
    registerConcurrencyBuiltins(interp);
}
