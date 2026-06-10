# Variables and Data Types in Depth

EZ is dynamically typed. You do not need to specify what type a variable is. The virtual machine determines the type at runtime and allows variables to change types freely.

## 1. Primitives (Passed by Value)

### Integers (`int`)
Standard 64-bit signed integers. They can handle extremely large numbers.
```ez
score = 150000
negativeBalance = -500
out "Total: " + (score + negativeBalance)
```

### Floats (`float` / `double`)
64-bit IEEE 754 floating-point numbers.
```ez
pi = 3.14159265
radius = 5.5
area = pi * (radius * radius)
```

### Booleans (`bool`)
EZ supports four keywords for booleans for readability: `true`, `false`, `yes`, `no`.
```ez
isServerRunning = yes
hasErrors = false

when isServerRunning {
    out "Server is online!"
}
```

### Strings
Strings are UTF-8 encoded and immutable.
```ez
firstName = "John"
lastName = "Doe"
fullName = firstName + " " + lastName

// You can use escape characters
out "Line 1\nLine 2\tTabbed"
```

## 2. Composites (Passed by Reference)

### Arrays
Dynamically sized, mutable lists that can contain mixed types.
```ez
inventory = ["Sword", "Shield", 100, true]

// Access by index
firstItem = inventory[0] // "Sword"

// Append new items using the empty bracket syntax []
inventory[] = "Health Potion"

// Modify existing items
inventory[2] = 150
```

### Dictionaries
Key-value stores. Keys can be strings or integers.
```ez
user = {
    "username": "admin",
    "role": "superuser",
    "active": yes,
    "permissions": ["read", "write", "execute"]
}

out "User Role: " + user["role"]

// Modifying nested arrays inside dictionaries
user["permissions"][] = "delete"
```

## 3. The `nil` Keyword
`nil` represents the explicit absence of a value.
```ez
data = nil
when data == nil {
    out "Data has not been loaded yet."
}
```

## 4. Type Checking
You can dynamically check the type of any variable using the built-in `typeOf()` function.
```ez
out typeOf(123)       // "Integer"
out typeOf(3.14)      // "Number"
out typeOf("hello")   // "String"
out typeOf([1,2])     // "Array"
out typeOf({a:1})     // "Dictionary"
```

## 5. Edge Cases & Pitfalls
- **Type Coercion**: EZ strictly prevents implicit coercion between dissimilar types. Adding a String to an Integer `out "Age: " + 30` will stringify the integer, but performing math on mismatched types throws errors (e.g. `"5" * 5` is invalid).
- **Array Out of Bounds**: Accessing an array out of bounds (e.g., `arr[10]` when size is 2) will immediately throw a `fatal runtime exception` rather than returning `nil`, terminating the VM!
- **Missing Dictionary Keys**: Accessing a key that does not exist in a dictionary returns `nil`. It does *not* throw an error.
  ```ez
  missingData = user["password"] // returns nil
  ```
- **Float Precision limits**: Comparing floats using `==` is subject to standard IEEE 754 precision issues (e.g., `0.1 + 0.2 == 0.3` evaluates to `false`).
- **Pass by Reference**: Arrays, Dictionaries, and Models are passed by reference. Primitive types (int, float, bool, nil, string) are passed by value. Modifying a dictionary inside a function modifies the original dictionary!
