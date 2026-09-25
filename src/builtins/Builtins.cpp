#include "runtime/objects/EZObjects.h"
#include "builtins/Builtins.h"
#include "testing/TestRunner.h"

void registerBuiltins(RuntimeContext& interp) {
    // data
    registerArrayBuiltins(interp);
    registerCSVBuiltins(interp);
    registerDictBuiltins(interp);
    registerFPBuiltins(interp);
    registerJSONBuiltins(interp);
    registerTypeBuiltins(interp);

    // string
    registerStringBuiltins(interp);
    registerRegexBuiltins(interp);
    registerEncodingBuiltins(interp);

    // io
    registerIOBuiltins(interp);
    registerFileBuiltins(interp);
    registerConsoleBuiltins(interp);

    // net
    registerNetBuiltins(interp);
    registerHttpBuiltins(interp);

    // math
    registerMathBuiltins(interp);

    // time
    registerTimerBuiltins(interp);
    registerDateTimeBuiltins(interp);
    registerTimeBuiltins(interp);

    // core
    registerCoreBuiltins(interp);
    registerExceptionBuiltins(interp);
    registerGCBuiltins(interp);
    registerProcessBuiltins(interp);
    registerVMBuiltins(interp);
    registerConcurrencyBuiltins(interp);

    // buffer
    registerBufferBuiltins(interp);

    // ffi
    registerFFIBuiltins(interp);
    
    TestRunner::registerTestBuiltins(interp);
}
