# Interfaces and Native Structs

## 1. Interfaces
Interfaces are strict contracts. If a model claims to implement an interface but fails to provide the required tasks, EZ will throw a compilation error.

```ez
interface Logger {
    task log(msg)
    task error(msg)
}

model ConsoleLogger implements Logger {
    task log(msg) {
        out "[INFO] " + msg
    }
    
    task error(msg) {
        out "[ERROR] " + msg
    }
}
```

## 2. Native Structs (`struct`)
Unlike dynamic `models`, `structs` map directly to raw memory. They are primarily used for Foreign Function Interface (FFI) calls to interact with C/C++ libraries (like the Windows API).

### Struct Definition
You must explicitly declare the data types for fields in a struct.
```ez
struct POINT {
    x: int
    y: int
}

struct RECT {
    left: int
    top: int
    right: int
    bottom: int
}
```

Supported Types:
- `int`: 32-bit signed integer
- `int64`: 64-bit signed integer
- `float`: 32-bit floating point
- `double`: 64-bit floating point
- `byte`: 8-bit unsigned integer
- `ptr`: 64-bit pointer/handle

### Instantiating and Accessing
Structs are instantiated with `new`. Internally, the VM allocates a contiguous `Buffer` to back the struct, ensuring it is 100% ABI compatible with C libraries.
```ez
p = new POINT()
p.x = 1920
p.y = 1080

// You can pass 'p' directly to an os_call!
```

## 3. Edge Cases & Pitfalls
- **Struct Memory Alignment**: Structs in EZ enforce rigorous C-style memory alignment (Padding). For example, a `byte` followed by an `int64` will introduce 7 bytes of invisible padding. Always match the layout exactly as expected by the C-library!
- **Null Pointers in Structs**: Be careful when setting a `ptr` field to `0`. If an external C library attempts to dereference a null pointer provided by your struct, it will crash the VM with an `Access Violation (0xC0000005)` rather than throwing an EZ exception.
- **Struct vs Model Mixups**: You cannot define tasks/methods inside a `struct`. Structs are strictly for Plain Old Data (POD) objects.
- **Interface Implementation Overloading**: EZ does not support method overloading. You must match the interface method name exactly, but argument counts are currently loosely validated at runtime.
