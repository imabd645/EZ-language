# Exhaustive Built-in Libraries Reference

EZ ships with a massive standard library exposed as global functions.

## 1. Console & File I/O
```ez
// Standard Console Output
out "Hello Console"
print("Multiple", "arguments", "supported")

// Console Input
name = input("Enter your name: ")
out "Hello, " + name

// Terminal formatting (Windows Console API)
color(10) // Green text
out "Success!"
reset()   // Reset color
clear()   // Clear screen
gotoxy(10, 5) // Move cursor to X:10, Y:5
getch()   // Pause and wait for single keystroke

// File I/O -- whole-file helpers
writeFile("test.txt", "Hello File System!")
content = readFile("test.txt")
out content
appendFile("log.txt", "one more line\n")
lines = readLines("test.txt")
```

For streaming, appending over time, seeking, or managing files on disk, use the
`File` class:

```ez
f = File("app.log", "a")     // "r" "w" "a", binary "rb" "wb" "ab", or "rw"
f.write("a line\n")
f.flush()                    // without this, recent writes sit in the buffer
out str(f.size())            // bytes on disk; position is preserved
f.close()
```

`File()` **throws** when the path cannot be opened — it never hands back a closed
handle, so test it with `try`, not by checking the return value:

```ez
try {
    f = File("missing/dir/x.txt", "a")
} catch (e) {
    out "could not open: " + str(e)
}
```

Path-level operations are statics:

```ez
File.exists(path)            // bool
File.size(path)              // bytes
File.rename(from, to)        // replaces the destination
File.remove(path)            // false if already absent, so no guard needed
File.delete(path)            // alias for remove
```

## 2. System & Process
```ez
clock()      // Float timestamp (e.g., 170000000.123)
stop(500)    // Sleep thread for 500ms
exit(1)      // Exit application with error code 1
panic("A fatal unrecoverable error occurred") 
```

## 3. Math API
Standard numeric operations.
```ez
out floor(4.9)   // 4
out ceil(4.1)    // 5
out round(4.5)   // 5
out abs(-100)    // 100
out sqrt(64)     // 8.0
out pow(2, 8)    // 256.0
out min(10, 20)  // 10
out max(10, 20)  // 20

// Randomization
out rand()         // Float between 0.0 and 1.0
out randint(1, 10) // Integer between 1 and 10
```

## 4. String Manipulation
Strings in EZ are immutable. All transformation functions return a new string.
```ez
str = "  EZ Language is powerful  "

out len(str)                           // Character count
out trim(str)                          // "EZ Language is powerful"
out upper(str)                         // Uppercase
out lower(str)                         // Lowercase
out substr("Hello World", 0, 5)        // "Hello"
out replace("apple", "p", "b")         // "abble"
out startsWith("EZ Lang", "EZ")        // true
out endsWith("EZ Lang", "Lang")        // true

// ASCII Code conversions
out ord("A") // 65
out chr(65)  // "A"

// Array to String conversions
arr = split("apple,banana,cherry", ",") // ["apple", "banana", "cherry"]
out join(arr, " | ")                    // "apple | banana | cherry"
```

### Regular Expressions (C++ `std::regex` wrapper)

Every regex builtin takes the **subject first, the pattern second**.

```ez
text = "Contact us at support@example.com"
email = "[\\w.-]+@[\\w.-]+"

// re_test = "does it occur anywhere?"
when re_test(text, email) {
    out "Contains an email!"
}

// reMatch is a FULL match -- the whole string must match
reMatch(text, email)          // false
reMatch("a@b.com", email)     // true

// reSearch returns [whole, group1, ...] for the first match
reSearch(text, email)         // ["support@example.com"]

// reReplace replaces every match
reReplace(text, email, "[REDACTED]")   // "Contact us at [REDACTED]"
```

The `re_*` family adds match **positions**, capture groups, flags and split —
none of which the three functions above provide:

```ez
m = re_find("2026-08-07", "([0-9]{4})-([0-9]{2})")
m["text"]        // "2026-08"
m["start"]       // 0
m["groups"][0]   // "2026"

re_find_all(text, email)               // every match, with offsets
re_split("a1b22c", "[0-9]+")           // ["a", "b", "c"]
re_replace("John Smith", "(\\w+) (\\w+)", "$2, $1")   // "Smith, John"
re_test("HELLO", "hello", "i")         // true -- "i" and "m" flags
re_escape("a.b")                       // match punctuation literally
re_valid("((")                         // false
```

An invalid pattern throws `RegexError` rather than quietly reporting "no match".
A capture group that did not participate is `nil`, not `""`.

Full reference: [BUILTINS.md](../BUILTINS.md#re_find).

## 5. Networking (libcurl Integration)
EZ provides a zero-setup networking library capable of handling SSL and complex HTTP requests.
```ez
// Synchronous GET
html = http_get("https://example.com")

// Synchronous POST
response = http_post("https://api.example.com/data", "{\"name\":\"EZ\"}")

// Asynchronous Fetch (Returns a Future)
options = {
    "method": "GET",
    "headers": ["Authorization: Bearer Token", "Accept: application/json"],
    "insecure": false 
}
future = fetch("https://api.example.com", options)
result = await(future) // wait for response without blocking GUI
out result
```

## 6. FFI (Foreign Function Interface) & Raw Memory
EZ can interact with native C-compiled DLLs directly. This is extremely powerful but highly dangerous!

### FFI Example: Calling Windows API `MessageBoxA`
```ez
// 1. Load the native Windows DLL
user32 = os_load_lib("user32.dll")

// 2. Extract the function pointer
MessageBoxA = os_get_func(user32, "MessageBoxA")

// 3. Allocate a string buffer in raw memory for the title and text
titlePtr = os_alloc(256)
textPtr = os_alloc(256)
os_write_string(titlePtr, 0, "FFI Alert")
os_write_string(textPtr, 0, "Hello directly from user32.dll!")

// 4. Call the function: HWND (0), Text (ptr), Title (ptr), Type (0)
// os_call(funcPtr, returnType, args...)
result = os_call(MessageBoxA, "int", 0, textPtr, titlePtr, 0)

// 5. CRITICAL: Free the unmanaged memory!
os_free(titlePtr)
os_free(textPtr)
```

## 7. Edge Cases & Pitfalls
- **Regex Invalid Syntax**: Passing an invalid regular expression string to `reMatch` or `reSearch` will cause a runtime exception to bubble up from the internal regex engine. Always wrap user-supplied regex strings in a `try/catch`.
- **String Out of Bounds**: Calling `substr` or `substring` with indices larger than the string length will result in an "out of bounds" fatal error. Always check `len()` first!
- **Network Timeouts**: The `http_get` and `fetch` calls have an internal timeout (usually 30 seconds). If a server hangs, the call will eventually throw an exception indicating a network failure.
- **FFI Memory Leaks**: Memory allocated via `os_alloc` completely bypasses the EZ Garbage Collector. If you fail to call `os_free()`, your script will leak memory indefinitely.
- **Access Violation (0xC0000005)**: Using `os_read_byte`, `os_write_byte`, or passing bad pointers to `os_call` will trigger an OS-level Access Violation, terminating the VM instantly and bypassing all `try/catch` blocks.
