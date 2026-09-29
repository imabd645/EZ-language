# Variables and Data Types in Depth

EZ is dynamically typed. You do not need to specify what type a variable is. The virtual machine determines the type at runtime and allows variables to change types freely.
Statements can be separated by a newline or an optional semicolon (`;`).

```ez
// Single line C-style comment
# Single line Hash comment
/* Block comments
   can span multiple lines */
a = 1; b = 2; c = 3
```

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
Strings are UTF-8 encoded and immutable. Note that `len()` returns the **byte length**, not the codepoint count (e.g. `len("café")` returns `5` because `é` is 2 bytes in UTF-8).
```ez
firstName = "John"
lastName = "Doe"
fullName = firstName + " " + lastName

// You can use escape characters
out "Line 1\nLine 2\tTabbed"
```

EZ also supports advanced string literals:
- **Raw Strings (`r"..."`)**: Ignores escape sequences (ideal for file paths). `r"C:\Users\file.txt"`
- **Template Strings**: Use backticks for interpolation: `` `Hello {firstName}, you are {age} years old` ``.
- **Multiline Strings**: Use triple quotes `"""..."""` to safely span multiple lines.

## 2. Composites (Passed by Reference)

### Arrays
Dynamically sized, mutable lists that can contain mixed types.
```ez
inventory = ["Sword", "Shield", 100, true]

// Access by index
firstItem = inventory[0] // "Sword"

// Append new items using push()
push(inventory, "Health Potion")
push(inventory, "Magic Scroll")

// Modify existing items
inventory[2] = 150

// Use the Spread Operator (...) to merge arrays
extraItems = ["Ring", "Amulet"]
mergedInventory = [...inventory, "Gold", ...extraItems]
```
Note: Negative indexing (`arr[-1]`) and bracket slicing (`arr[1:3]`) are not supported. Use `arr[len(arr)-1]` and `slice(arr, 1, 3)` instead.

### Dictionaries
Key-value stores. Keys are always internally stored as strings. If you pass an Integer, Float, or Boolean as a key, it will be automatically coerced into a string. Passing mutable objects like Arrays or Dictionaries as keys will throw a `TypeError`.
```ez
user = {
    "username": "admin",
    "role": "superuser",
    "active": yes,
    "permissions": ["read", "write", "execute"]
}

out "User Role: " + user["role"]

// Modifying nested arrays inside dictionaries
push(user["permissions"], "delete")
```

### Tuples
EZ supports lightweight, immutable tuples. They are created using parentheses and are highly useful for multi-variable destructuring.
```ez
t = (1, 2, "hello")
(a, b, c) = someTaskReturningTuple()
```

## 3. The `nil` Keyword
`nil` represents the explicit absence of a value.
```ez
data = nil
when data == nil {
    out "Data has not been loaded yet."
}
```

### Nil-Coalescing Operator (`??`)
You can safely provide a default value when a variable is `nil` using the `??` operator. If the left side is `nil`, it evaluates and returns the right side.
```ez
username = nil
displayName = username ?? "Anonymous User"
out displayName // "Anonymous User"
```

### Optional Chaining (`?.`)
When accessing deeply nested dictionaries or objects that might be `nil`, use `?.` to safely short-circuit instead of throwing a runtime error.
```ez
user = { "profile": nil }

// Safely access name. If profile is nil, the expression evaluates to nil!
name = user?.profile?.name ?? "Unknown"
out name // "Unknown"
```

## 4. Type Checking
You can dynamically check the type of any variable using the built-in `typeOf()` function.
```ez
out typeOf(123)       // "integer"
out typeOf(3.14)      // "float"
out typeOf("hello")   // "string"
out typeOf([1,2])     // "array"
out typeOf({a:1})     // "dictionary"
```

## 5. Edge Cases & Pitfalls
- **Type Coercion Flexibility**: EZ supports dynamic coercion for specific operators:
  - Adding a string to an array or primitive (e.g., `[1, 2] + "hello"`) coerces the left side and concatenates it (`"[1, 2]hello"`).
  - Multiplying a string by an integer (e.g., `"hello" * 3`) repeats the string (`"hellohellohello"`).
  - Attempting truly ambiguous math operations (like `[1, 2] * 3` or `Dict + Dict`) will be statically rejected by the Typechecker.
- **Bitwise Float Truncation**: When applying bitwise operators (`&`, `|`, `^`, `~`) to floats, EZ does not throw an error. Instead, it performs implicit C-style truncation to integers (e.g., `5.5 & 3.1` evaluates to `1`).
- **Array Out of Bounds**: Accessing an array out of bounds (e.g., `arr[10]` when size is 2) will immediately throw a `fatal runtime exception` rather than returning `nil`, terminating the VM!
- **Missing Dictionary Keys**: Accessing a key that does not exist in a dictionary returns `nil`. It does *not* throw an error.
  ```ez
  missingData = user["password"] // returns nil
  ```
- **Float Precision limits**: Comparing floats using `==` is subject to standard IEEE 754 precision issues (e.g., `0.1 + 0.2 == 0.3` evaluates to `false`).
- **Pass by Reference**: Arrays, Dictionaries, and Models are passed by reference. Primitive types (int, float, bool, nil, string) are passed by value. Modifying a dictionary inside a function modifies the original dictionary!

