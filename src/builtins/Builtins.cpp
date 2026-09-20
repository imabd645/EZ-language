#include "runtime/objects/EZObjects.h"
#include "builtins/Builtins.h"
#include "testing/TestRunner.h"

void registerBuiltins(RuntimeContext& interp) {
    registerIOBuiltins(interp);
    registerNetBuiltins(interp);
    registerMathBuiltins(interp);
    registerStringBuiltins(interp);
    registerDataBuiltins(interp);
    registerCoreBuiltins(interp);
    registerConsoleBuiltins(interp);
    registerFFIBuiltins(interp);
    registerBufferBuiltins(interp);
    registerHttpBuiltins(interp);
    registerTimeDateBuiltins(interp);
    
    // Modularized Builtins
    registerGCBuiltins(interp);
    registerVMBuiltins(interp);
    registerProcessBuiltins(interp);
    registerExceptionBuiltins(interp);
    registerTimeBuiltins(interp);
    registerConcurrencyBuiltins(interp);

    TestRunner::registerTestBuiltins(interp);
}
