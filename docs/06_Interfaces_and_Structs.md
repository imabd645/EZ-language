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

## 2. Structs (`struct`)
A `struct` is a Plain Old Data type: a named set of typed fields with a generated
constructor and no methods.

> **Not currently ABI-compatible.** Earlier versions of this page described structs as
> mapping "directly to raw memory", backed by a contiguous `Buffer`, and safe to pass
> straight to `os_call`. No such implementation exists — there is no buffer backing, no
> layout computation and no FFI marshalling anywhere in the runtime. A struct instance is
> an ordinary object (`typeOf` reports `instance`). To call a C function that takes a
> struct, build the bytes yourself with `buffer()` and the `os_read_*`/`os_write_*`
> helpers. The field type names below are recorded and type-checked, but they do not yet
> determine a memory layout.

### Struct Definition
You may annotate each field with a type. The annotation is checked, and selects the
zero value used when the field is omitted at construction.
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

Field types and the zero value each implies when the field is omitted:

| Type | Zero |
|---|---|
| `int`, `int64`, `byte` | `0` |
| `float`, `double` | `0.0` |
| `ptr` | `0` |
| `string` | `""` |
| `bool` | `false` |
| omitted / `Any` | `nil` |

### Instantiating and Accessing
Structs are instantiated with `new`. Fields may be passed positionally in declaration
order, and any left out take their default (or the zero above).

```ez
p = new POINT()             // x = 0, y = 0
p.x = 1920
p.y = 1080

q = new POINT(1920, 1080)   // positional, in field order
```

An explicit default overrides the type's zero:

```ez
struct Config {
    name: string = "default"
    retries: int = 3
    ratio: float
}

c = new Config()            // name "default", retries 3, ratio 0.0
d = new Config("custom", 9) // name "custom",  retries 9, ratio 0.0
```

## 3. Edge Cases & Pitfalls
- **Structs are objects, not memory**: a struct instance behaves like a model instance.
  It cannot be handed to `os_call` as a C struct — see the note above.
- **Struct vs Model Mixups**: You cannot define tasks/methods inside a `struct`. Structs are strictly for Plain Old Data (POD) objects.
- **Interface Implementation Overloading**: EZ does not support method overloading. You must match the interface method name exactly, but argument counts are currently loosely validated at runtime.
