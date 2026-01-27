# EZ Language Specification

## 1. Lexical Structure

### Keywords
The following identifiers are reserved words:
`task`, `give`, `when`, `other`, `while`, `repeat`, `get`, `escape`, `skip`, `model`, `init`, `self`, `hidden`, `shown`, `extends`, `out`, `in`, `try`, `catch`, `throw`, `use`, `to`, `true`, `false`, `nil`.

### Operators
- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Comparison: `==`, `!=`, `<`, `<=`, `>`, `>=`
- Logical: `and`, `or`, `not` (`!`)
- Assignment: `=`, `+=`, `-=`, `*=`, `/=`
- Member Access: `.`, `[]`
- Lambda: `|`, `=>`

### Literals
- **Numbers**: Double-precision floating point.
- **Strings**: Single (`'`) or double (`"`) quoted.
- **Booleans**: `true`, `false`
- **Nil**: `nil`

## 2. Grammar

### Statements
Program consists of a sequence of statements.

```ebnf
program      ::= statement*
statement    ::= declaration | expression_stmt | control_flow
declaration  ::= var_decl | task_decl | model_decl
```

### Control Flow
EZ uses `when` for conditionals.

```javascript
when condition {
    body
} other when condition2 {
    body
} other {
    body
}
```

Loops include `while`, `repeat`, and `get`.

```javascript
# Standard While
while condition { ... }

# Range Loop (inclusive)
repeat var = start to end { ... }

# Collection Iterator
get item in collection { ... }
```

### Functions
Declared with `task`. Implicit returns are not supported; use `give`.

```javascript
task name(p1, p2) {
    ...
    give value
}
```

### Classes (Models)
Declared with `model`.

```javascript
model Name extends Parent {
    hidden privateVar
    shown publicVar
    
    init(args) { ... }
    
    task method() { ... }
}
```

## 3. Type System
EZ is dynamically typed. Value types include:
- `Nil`: Represents absence of value.
- `Boolean`
- `Number`
- `String`
- `Array`: Dynamic size, mixed types.
- `Dictionary`: String keys, mixed values.
- `Function`: First-class callable.
- `Class`: Blueprint for instances.
- `Instance`: Object created from a class.
- `Future`: Handle for an asynchronous operation.
