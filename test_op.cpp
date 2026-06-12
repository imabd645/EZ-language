#include <iostream>
#include "src/Bytecode.h"
int main() {
    std::cout << "CALL: " << (int)OpCode::CALL << std::endl;
    std::cout << "OP_AWAIT: " << (int)OpCode::OP_AWAIT << std::endl;
    std::cout << "POP: " << (int)OpCode::POP << std::endl;
    std::cout << "PRINT: " << (int)OpCode::PRINT << std::endl;
    return 0;
}
