#include "FFI_Internal.h"

// ============================================================================
// FFI entry point. Keeps the name and signature declared in Builtins.h so
// Builtins.cpp is unchanged; the work now lives in the four group files.
// ============================================================================

void registerFFIBuiltins(RuntimeContext& interp) {
    registerFFIMemory(interp);
    registerFFICall(interp);
    registerFFIStruct(interp);
    registerFFICallback(interp);
}
