# 🔌 Native FFI (Foreign Function Interface)

EZ provides a raw, highly capable Foreign Function Interface (FFI). It allows you to dynamically load system libraries (`.dll`, `.so`, `.dylib`), allocate memory, execute arbitrary C ABI functions, and define C-compatible structs. 

> **Security Note:** Because the FFI grants raw memory access, it is completely disabled when executing under `--safe` mode unless you explicitly grant the `--allow-ffi` capability.

---

## 1. Loading Libraries & Finding Functions

Use `os_load_lib` to dynamically load a shared library into the VM process, and `os_get_func` to locate a function pointer.

```ez
// 1. Load the library (Windows example)
user32 = os_load_lib("user32.dll")

// 2. Find the function pointer
msgBox = os_get_func(user32, "MessageBoxA")

// 3. Free the library when done
os_free_lib(user32)
```

---

## 2. Calling Functions (`os_call` & `os_call_sig`)

The FFI supports automatically inferring C types from EZ types, but you must specify the **Return Type** as a string (e.g., `"int"`, `"void"`, `"ptr"`, `"string"`, `"float"`).

### Basic Inference (`os_call`)
```ez
// Signature: os_call(funcPtr, returnType, ...args)
result = os_call(msgBox, "int", 0, "Hello from EZ!", "FFI Dialog", 0)
```

### Explicit Signatures (`os_call_sig`)
If automatic inference gets it wrong (e.g., you need to pass an integer as a 64-bit float, or a specific pointer width), you can supply a signature array.

```ez
// Signature: os_call_sig(funcPtr, returnType, [argTypes...], ...args)
result = os_call_sig(msgBox, "int", ["ptr", "string", "string", "int"], 0, "Hello", "Title", 0)
```

---

## 3. Raw Memory Allocation (`os_alloc` & `os_free`)

You can allocate raw bytes on the C-heap. EZ will return the memory address as an integer. Memory allocated this way is **not** garbage collected—you must manually free it.

```ez
// Allocate 1024 bytes
ptr = os_alloc(1024)

// ... use the memory ...

// Free it
os_free(ptr)
```

---

## 4. Reading and Writing Memory

EZ provides a complete suite of typed memory access functions. 
> **Safety:** All memory operations are wrapped in safe exception handlers (`SAFE_MEMORY_OP`). If you accidentally read an invalid pointer, it throws a catchable `FFI memory access violation` instead of fatally crashing the interpreter!

### Integer Access
*   `os_read_uint64(ptr, offset)` / `os_write_uint64(ptr, offset, val)`
*   `os_read_int64(ptr, offset)` / `os_write_int64(ptr, offset, val)`
*   `os_read_uint32(ptr, offset)` / `os_write_uint32(ptr, offset, val)`
*   `os_read_int32(ptr, offset)` / `os_write_int32(ptr, offset, val)`
*   `os_read_uint16(ptr, offset)` / `os_write_uint16(ptr, offset, val)`
*   `os_read_int16(ptr, offset)` / `os_write_int16(ptr, offset, val)`
*   `os_read_byte(ptr, offset)` / `os_write_byte(ptr, offset, val)`

### Floating Point Access
*   `os_read_float(ptr, offset)` / `os_write_float(ptr, offset, val)`
*   `os_read_double(ptr, offset)` / `os_write_double(ptr, offset, val)`

### String Access
*   `os_read_string_ptr(ptr)`: Reads a null-terminated UTF-8 string starting at `ptr`.
*   `os_read_string_ptr_n(ptr, length)`: Reads up to `length` bytes (stops at NUL).
*   `os_write_string(ptr, offset, "text")`: Writes string bytes (plus null terminator).

---

## 5. C Structs

Working with raw offsets is tedious. EZ provides a struct packer/unpacker using arrays of field type names (`i8`, `u8`, `i16`, `u16`, `i32`, `u32`, `f32`, `i64`, `u64`, `f64`, `ptr`) following standard C alignment and padding rules.

```ez
// Pack values into a byte buffer
layout = ["i32", "f32", "i64"]
buffer = os_struct_pack(layout, [42, 3.14, 9000000])

// Unpack bytes back into an array
values = os_struct_unpack(layout, buffer)
out values[0] // 42
```

Use `os_struct_alloc(layout)` to allocate a zeroed byte `Buffer` sized for that exact struct layout.

---

## 6. FFI Callbacks

Sometimes C code needs to execute an EZ task (e.g., GUI event loops, hooks). You can wrap an EZ task into a raw C function pointer.

```ez
task myCallback(hwnd, msg, wparam, lparam) {
    out "Received message: " + msg
    give 0
}

// Wrap the task: os_ffi_create_callback(sigArray, returnType, fn)
c_ptr = os_ffi_create_callback(["ptr", "u32", "ptr", "ptr"], "int", myCallback)

// Pass c_ptr to C libraries...

// Clean up when done to prevent memory leaks
os_ffi_free_callback(c_ptr)
```
