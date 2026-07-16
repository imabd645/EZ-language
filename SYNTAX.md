# SYNTAX.md - EZ Language Keywords and Operators

## Keywords

All keywords are reserved and cannot be used as identifiers.

### Control Flow

| Keyword | Description |
|---------|-------------|
| `out` | Print output to console |
| `in` | Input from console |
| `when` | Conditional statement (if) |
| `other` | Else clause for when |
| `while` | While loop |
| `repeat` | For loop |
| `to` | Loop range delimiter |
| `step` | Loop step increment |
| `match` | Pattern matching |
| `escape` | Break statement |
| `skip` | Continue statement |
| `get` | Property getter |

### Boolean and Logic

| Keyword | Description |
|---------|-------------|
| `and` | Logical AND |
| `or` | Logical OR |
| `not` | Logical NOT |
| `true` | Boolean true |
| `false` | Boolean false |
| `yes` | Boolean true (alias) |
| `no` | Boolean false (alias) |
| `nil` | Null/nil value |

### Async and Concurrency

| Keyword | Description |
|---------|-------------|
| `async` | Async function/task |
| `await` | Await a future |
| `task` | Task definition |
| `give` | Return value from async task |

### Modules

| Keyword | Description |
|---------|-------------|
| `use` | Import module |
| `export` | Export symbol |

### Error Handling

| Keyword | Description |
|---------|-------------|
| `try` | Try block |
| `catch` | Catch exception |
| `throw` | Throw exception |
| `finally` | Finally block |

### Object-Oriented Programming

| Keyword | Description |
|---------|-------------|
| `model` | Class definition |
| `init` | Constructor |
| `self` | This/self reference |
| `hidden` | Private visibility |
| `shown` | Public visibility |
| `extends` | Inheritance |
| `struct` | Struct definition |
| `new` | Instance creation |
| `super` | Super class reference |
| `static` | Static member |
| `interface` | Interface definition |
| `implements` | Interface implementation |

### Contracts

| Keyword | Description |
|---------|-------------|
| `requires` | Precondition |
| `ensures` | Postcondition |

### Decorators

| Keyword | Description |
|---------|-------------|
| `decorator` | Decorator keyword |

## Operators

### Arithmetic Operators

| Operator | Description |
|----------|-------------|
| `+` | Addition |
| `-` | Subtraction |
| `*` | Multiplication |
| `/` | Division |
| `%` | Modulo |
| `+=` | Add and assign |
| `-=` | Subtract and assign |
| `*=` | Multiply and assign |
| `/=` | Divide and assign |

### Comparison Operators

| Operator | Description |
|----------|-------------|
| `==` | Equal |
| `!=` | Not equal |
| `<` | Less than |
| `<=` | Less than or equal |
| `>` | Greater than |
| `>=` | Greater than or equal |

### Logical Operators

| Operator | Description |
|----------|-------------|
| `and` | Logical AND (keyword) |
| `or` | Logical OR (keyword) |
| `not` | Logical NOT (keyword) |
| `!` | Logical NOT (operator) |

### Bitwise Operators

| Operator | Description |
|----------|-------------|
| `&` | Bitwise AND |
| `|` | Bitwise OR |
| `^` | Bitwise XOR |
| `~` | Bitwise NOT |
| `<<` | Left shift |
| `>>` | Right shift |

### Assignment Operators

| Operator | Description |
|----------|-------------|
| `=` | Assignment |
| `+=` | Add and assign |
| `-=` | Subtract and assign |
| `*=` | Multiply and assign |
| `/=` | Divide and assign |

### Other Operators

| Operator | Description |
|----------|-------------|
| `.` | Member access |
| `..` | Invalid token (not allowed) |
| `...` | Ellipsis (spread) |
| `:` | Colon (type annotation) |
| `@` | At symbol (decorator) |
| `?` | Optional chaining / null coalescing |
| `??` | Null coalescing operator |
| `?.` | Optional chaining operator |
| `->` | Arrow (function type or lambda) |
| `=>` | Arrow (alternative syntax) |

## Punctuation

| Symbol | Description |
|--------|-------------|
| `(` `)` | Parentheses (grouping, function calls) |
| `[` `]` | Brackets (array indexing, array literals) |
| `{` `}` | Braces (dict literals, blocks) |
| `,` | Comma (separator) |
| `;` | Semicolon (statement terminator, optional) |
| `\n` | Newline (statement separator) |

## Literals

### String Literals

- **Single-quoted**: `'Hello'`
- **Double-quoted**: `"Hello"`
- **Multiline**: `"""Hello\nWorld"""`
- **Raw strings**: `r"Hello\nWorld"` (no escape sequences)
- **Template strings**: `` `Hello {name}` `` (with interpolation)

### Number Literals

- **Integer**: `42`, `-123`
- **Float**: `3.14`, `-0.5`
- **Hexadecimal**: `0xFF`, `0xABC`
- **Scientific notation**: `1.5e10`, `2.5E-3`

### Boolean Literals

- `true` 
- `false`

### Nil Literal

- `nil`

## Comments

### Line Comments

```ez
# This is a line comment
// This is also a line comment
```

### Block Comments

```ez
/* This is a
   multi-line
   block comment */
```

Nested block comments are supported:
```ez
/* Outer comment
   /* Nested comment */
   Still outer */
```

## Identifiers

Identifiers must start with a letter or underscore, followed by letters, digits, or underscores.

**Valid identifiers:**
- `myVariable`
- `_private`
- `camelCase`
- `PascalCase`
- `snake_case`
- `UPPER_SNAKE_CASE`

**Invalid identifiers:**
- `123variable` (starts with digit)
- `my-variable` (contains hyphen)
- `my variable` (contains space)

## Precedence (Highest to Lowest)

1. Parentheses `()`
2. Member access `.`, optional chaining `?.`, indexing `[]`
3. Unary operators `!`, `-`, `~`, `not`
4. Multiplication `*`, `/`, `%`
5. Addition `+`, subtraction `-`
6. Bit shifts `<<`, `>>`
7. Comparison `<`, `<=`, `>`, `>=`
8. Equality `==`, `!=`
9. Bitwise AND `&`
10. Bitwise XOR `^`
11. Bitwise OR `|`
12. Logical AND `and`
13. Logical OR `or`
14. Null coalescing `??`
15. Assignment `=`, `+=`, `-=`, `*=`, `/=`

## Source Reference

All keywords and operators are defined in `src/lexer/Lexer.cpp`:
- Keywords map (lines 5-52)
- Operator tokenization (lines 98-282)
