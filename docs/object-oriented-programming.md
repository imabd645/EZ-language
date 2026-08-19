# Object-Oriented Programming

EZ's OOP system centers on `model` (classes), with interfaces, structs,
enums, static members, and full operator overloading.

## Models (classes)

```ez
model Animal {
    init(name, sound) {
        self.name  = name
        self.sound = sound
    }

    # Public method
    shown speak() {
        out self.name + " says " + self.sound
    }

    # Private method
    hidden _describe() {
        give "I am " + self.name
    }

    # toString override — called by out and str()
    task toString() {
        give "Animal(" + self.name + ")"
    }
}

cat = Animal("Cat", "meow")
cat.speak()          # Cat says meow
out str(cat)         # Animal(Cat)
```

`init(...)` is the constructor. Methods default to `task name(...)`;
`shown`/`hidden` are explicit visibility markers. A `toString()` method, if
defined, is called by `out` and `str()` when printing an instance.

## Inheritance

```ez
model Dog extends Animal {
    init(name) {
        super.init(name, "woof")
        self.tricks = []
    }

    task learnTrick(trick) {
        push(self.tricks, trick)
        give self    # enables method chaining
    }

    shown perform() {
        get trick in self.tricks {
            out self.name + " performs: " + trick
        }
    }
}

rex = Dog("Rex")
rex.learnTrick("sit").learnTrick("roll over")
rex.speak()      # Rex says woof
rex.perform()    # Rex performs: sit
                 # Rex performs: roll over
```

When a class `extends` a parent, all of the parent's methods are copied into
the child's method table **at class-creation time** (unless overridden) —
inheritance is a one-time method-table merge, not a runtime lookup chain
that walks up a class hierarchy on every call. `super.method(...)` calls the
parent's implementation explicitly.

## Interfaces

```ez
interface Serializable {
    task toJson()
    task fromJson(json)
}

model Config implements Serializable {
    init(data) {
        self.data = data
    }

    task toJson() {
        give to_json(self.data)
    }

    task fromJson(json) {
        self.data = parse_json(json)
        give self
    }
}

cfg = Config({"debug": true, "port": 3000})
out cfg.toJson()
```

When a model declares `implements SomeInterface`, the VM checks (at
class-creation time) that every method named in the interface exists in the
class's method table. A missing method raises:

```
Model 'X' fails to implement interface 'Serializable': missing task 'fromJson'
```

This is **presence-only** validation — parameter and return types of
interface methods are not checked at runtime, even if the interface
declares them with type annotations.

## Static members

```ez
model Counter {
    static count = 0

    init() {
        Counter.count += 1
        self.id = Counter.count
    }

    static task reset() {
        Counter.count = 0
    }

    static task total() {
        give Counter.count
    }
}

a = Counter()
b = Counter()
c = Counter()
out str(Counter.total())    # 3
Counter.reset()
out str(Counter.total())    # 0
```

Static fields and methods are shared across all instances and accessed via
`Model.member`, never `self.member`.

## Structs

```ez
struct Point {
    x, y
}

p = new Point()
p.x = 10
p.y = 20
out str(p.x) + ", " + str(p.y)
```

Structs are lightweight field-only records, instantiated with `new`. Under
the [type checker](#type-checker-integration), fields may carry type
annotations and default values:

```ez
struct User {
    name: string
    age: number = 25
}
```

## Enums

`enum` declares a set of named integer constants, read as `Name.MEMBER`:

```ez
enum Color { RED, GREEN, BLUE }          # 0, 1, 2
out str(Color.GREEN)                     # 1
```

Numbering starts at 0 and increments automatically. An explicit value
**re-seeds** the counter rather than restarting it:

```ez
enum Status {
    OK = 200,
    NOT_FOUND = 404,
    SERVER_ERROR = 500
}

enum Seeded { A = 5, B, C }              # 5, 6, 7
```

Commas are optional between members — newlines work too, and a trailing
comma is fine:

```ez
enum Direction {
    NORTH,
    EAST,
    SOUTH,
    WEST,
}
```

Members are ordinary numbers, so they compare, do arithmetic, and live in
arrays/dictionaries like any other value:

```ez
when code == Status.NOT_FOUND { out "missing" }
palette = [Color.RED, Color.GREEN]
```

Internally, an enum desugars to a `model` whose members are all `static` —
which is why the access syntax matches static members. Two mistakes are
rejected at **parse time**: a duplicate member name (would silently shadow
the earlier one) and an empty enum body.

## Operator overloading

The parser allows *any* token as a function name inside a `model` body —
this is what enables operator overloading. The VM checks for a matching
instance method before falling back to the built-in numeric/structural
behavior.

| Operator | Method name | Notes |
|---|---|---|
| `+` | `task +(other)` | |
| `-` (binary) | `task -(other)` | |
| `*` | `task *(other)` | |
| `/` | `task /(other)` | |
| `==` | `task ==(other)` | |
| `<` | `task <(other)` | |
| `>` | `task >(other)` | |
| `>=` | `task >=(other)` | |
| `<=` | `task <=(other)` | |
| `!=` | `task !=(other)` | |
| `-` (unary negation) | `task neg()` | invoked for `-v` |

```ez
model Vector {
    init(x, y) {
        self.x = x
        self.y = y
    }

    task +(other) {
        give Vector(self.x + other.x, self.y + other.y)
    }

    task ==(other) {
        give self.x == other.x and self.y == other.y
    }

    task neg() {
        give Vector(-self.x, -self.y)
    }

    task toString() {
        give "Vector(" + str(self.x) + ", " + str(self.y) + ")"
    }
}

v1 = Vector(10, 20)
v2 = Vector(5, 5)
v3 = v1 + v2          # Vector(15, 25)
v1 += v2              # desugars to v1 = v1 + v2
out str(v1 == v3)     # true
v4 = -v2              # Vector(-5, -5)
```

## Type checker integration

Under the optional [static type checker](features.md#5-optional-static-type-checker),
model methods, struct fields, and interface method signatures may all carry
type annotations:

```ez
struct User {
    name: string
    age: number = 25
}

interface Logger {
    task log(message: string, level: number) -> bool
}

model App {
    task start() -> bool { ... }
}
```

The checker validates `self` usage (only legal inside a model's `init` or
methods — `self` at top level is an error) and type-checks assignments and
call sites against these annotations, but this is entirely optional;
untyped models behave exactly as shown in the examples above.

## Exceptions as models

Because `throw` accepts any value, models are commonly used as typed
exceptions, matched by `catch (TypeName e)`:

```ez
model MathError {
    init(msg) { self.msg = msg }
}

try {
    throw MathError("division by zero")
} catch (MathError e) {
    out "Math error: " + e.msg
}
```

See [error-handling.md](error-handling.md) for the full exception-handling
reference, including subclass matching and the built-in `Exception` helper.
