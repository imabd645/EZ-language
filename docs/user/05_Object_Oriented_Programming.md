# 🏛️ Object-Oriented Programming (OOP)

EZ provides a modern, fast, class-based object-oriented programming model with single inheritance, constructor initialization, method overriding, and `super` calls.

---

## 1. 🏗️ Defining Classes & Constructors

Classes are defined with the `class` keyword. The `init` method serves as the constructor:

```ez
class Vector2D {
    task init(x, y) {
        self.x = x
        self.y = y
    }

    task length() {
        give (self.x ** 2 + self.y ** 2) ** 0.5
    }

    task add(other) {
        give Vector2D(self.x + other.x, self.y + other.y)
    }
}

// Instantiation
v1 = Vector2D(3, 4)
out "Length: " + str(v1.length()) // 5.0
```

---

## 2. 🧬 Inheritance & `super` Calls

Use the `<` syntax to inherit from a parent class. Call parent methods using `super.method()`:

```ez
class Animal {
    task init(name) {
        self.name = name
    }

    task speak() {
        out self.name + " makes a sound."
    }
}

class Dog < Animal {
    task init(name, breed) {
        super.init(name)
        self.breed = breed
    }

    task speak() {
        out self.name + " (" + self.breed + ") barks: Woof!"
    }
}

dog = Dog("Rex", "German Shepherd")
dog.speak() // Rex (German Shepherd) barks: Woof!
```

---

## 3. ⚡ Fast Direct Method Invocation

Method invocations like `obj.method(arg1, arg2)` are compiled into specialized `INVOKE_METHOD` bytecodes with **monomorphic inline caching**, avoiding intermediate closure object allocations and delivering near-native dispatch speeds.
