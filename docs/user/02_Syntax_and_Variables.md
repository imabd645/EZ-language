# 📜 Syntax & Data Types

This guide covers variable declarations, primitive types, collections, and scoping rules in EZ.

---

## 1. 🏷️ Variables & Assignment

Variables in EZ do not require `var` or `let` keywords. Variables are dynamically typed and initialized on first assignment:

```ez
name = "Antigravity"    // String
age = 25                // Integer
ratio = 3.14159         // Float (Double)
isActive = true         // Boolean (true / false)
nothing = nil           // Nil (null)
```

---

## 2. 🔢 Primitive Data Types

| Type | Examples | Description |
| :--- | :--- | :--- |
| **Integer** | `42`, `-10`, `0xFF`, `0b1010` | 64-bit signed integer. |
| **Number** | `3.14`, `-0.05`, `1e6` | 64-bit IEEE 754 floating-point. |
| **String** | `"Hello"`, `'Single quoted'` | UTF-8 encoded text string. |
| **Boolean** | `true`, `false`, `yes`, `no` | Boolean truth values. |
| **Nil** | `nil` | Represents the absence of a value. |

---

## 3. 📦 Collections

### Arrays (Lists)
Arrays are ordered, dynamic, 0-indexed sequences:
```ez
numbers = [1, 2, 3, 4, 5]
out numbers[0]          // 1
numbers.push(6)         // Append element
out len(numbers)        // 6
```

### Dictionaries (Hash Maps)
Dictionaries store key-value pairs with string or primitive keys:
```ez
user = {
    "name": "Alice",
    "role": "Admin",
    "active": true
}

out user["name"]        // "Alice"
out user.role           // Dot property access works for identifier keys!
```

### Tuples
Tuples are fixed-size ordered groups:
```ez
point = (10, 20, 30)
out point[0]            // 10
```

---

## 4. 🧮 Operators

```ez
// Arithmetic
sum = 10 + 5
diff = 20 - 4
prod = 6 * 7
quot = 25 / 2           // 12.5 (promotes to float)
modulo = 100 % 7        // 2
power = 2 ** 8          // 256

// Bitwise
mask = 0xFF & 0x0F      // 15
flags = 1 | 2 | 4       // 7
shifted = 1 << 4        // 16
inverted = ~0           // -1

// Comparisons & Logic
isValid = (age >= 18) and (isActive == true)
isExcluded = not (age < 18)
```
