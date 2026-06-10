# Interfaces and Native Structs

## Interfaces
Interfaces define a contract that models must adhere to. Use the `implements` keyword.

```ez
interface Flyable {
    task fly()
}

model Bird implements Flyable {
    task fly() {
        out "Bird is flying"
    }
}
```

## Native Structs
EZ allows defining C-compatible structs for FFI (Foreign Function Interface) interoperability. Structs operate on contiguous memory blocks and use static typing.

```ez
struct Point {
    x: int
    y: int
}

p = new Point()
p.x = 10
p.y = 20
```

Struct fields are strictly typed (`int`, `float`, `byte`, `ptr`, etc.) and mapped directly to memory, making them perfect for interacting with `os_call` and Windows APIs.
