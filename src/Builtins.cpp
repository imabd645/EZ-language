#include "Builtins.h"

void registerBuiltins(Interpreter& interp) {
    registerIOBuiltins(interp);
    registerNetBuiltins(interp);
    registerDBBuiltins(interp);
    registerMathBuiltins(interp);
    registerStringBuiltins(interp);
    registerDataBuiltins(interp);
    registerSysBuiltins(interp);
    registerPDFBuiltins(interp);
}
