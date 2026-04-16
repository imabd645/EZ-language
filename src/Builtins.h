#ifndef BUILTINS_H
#define BUILTINS_H

class Interpreter;

void registerBuiltins(Interpreter& interp);

void registerIOBuiltins(Interpreter& interp);
void registerNetBuiltins(Interpreter& interp);
void registerDBBuiltins(Interpreter& interp);
void registerMathBuiltins(Interpreter& interp);
void registerStringBuiltins(Interpreter& interp);
void registerDataBuiltins(Interpreter& interp);
void registerSysBuiltins(Interpreter& interp);
void registerPDFBuiltins(Interpreter& interp);
void registerBufferBuiltins(Interpreter& interp);

#endif // BUILTINS_H
