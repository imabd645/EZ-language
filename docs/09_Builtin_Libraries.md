# Built-in Libraries and APIs

EZ includes a rich set of built-in functions available globally.

## Core Functions
- `out <value>`: Prints to the console.
- `len(collection)`: Returns the length of an array, string, or dictionary.
- `typeOf(value)`: Returns the string representation of a value's type.
- `clock()`: Returns the current timestamp.

## String Manipulation
- `substr(str, start, length)`
- `split(str, delimiter)`
- `join(array, delimiter)`
- `upper(str)`, `lower(str)`, `trim(str)`
- `replace(str, old, new)`
- `startsWith(str, prefix)`, `endsWith(str, suffix)`
- **Regex**: `reMatch(pattern, str)`, `reSearch(pattern, str)`, `reReplace(pattern, replace, str)`

## Network APIs
EZ features built-in libcurl integration for networking.
- `http_get(url)`: Performs a GET request.
- `http_post(url, body)`: Performs a POST request.
- `fetch(url, options)`: Advanced async request returning a Future.
- `url_encode(str)`, `url_decode(str)`

## FFI and System Interop
- `os_load_lib(path)`: Loads a native DLL/so.
- `os_get_func(lib, name)`: Retrieves a function pointer.
- `os_call(funcPtr, returnType, [args])`: Calls a native C function.

Memory allocation for FFI:
- `os_alloc(size)`, `os_free(ptr)`
- `os_write_uint32(ptr, offset, value)`, `os_read_uint32(ptr, offset)`
