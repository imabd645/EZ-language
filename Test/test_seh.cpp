#include <windows.h>
#include <iostream>

int main() {
    __try {
        int* p = nullptr;
        *p = 42;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        std::cout << "Caught exception!" << std::endl;
    }
    return 0;
}
