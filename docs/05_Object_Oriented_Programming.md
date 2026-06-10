# Object-Oriented Programming (Models)

EZ uses `model` to provide a robust blueprint system for creating objects. Models support inheritance, private state encapsulation, and static methods.

## 1. Class Structure & Instantiation
The `init` task acts as the constructor. You bind properties to the instance using the `self` keyword.

```ez
model Vehicle {
    init(make, modelName) {
        self.make = make
        self.modelName = modelName
        self.speed = 0
    }

    task accelerate(amount) {
        self.speed = self.speed + amount
        out self.make + " is now going " + self.speed + " mph."
    }
}

car = new Vehicle("Toyota", "Corolla")
car.accelerate(30)
```

## 2. Encapsulation (`shown` vs `hidden`)
Models can restrict access to their internal properties and tasks. 
- `shown` (default): Publicly accessible from anywhere.
- `hidden`: Private. Can only be accessed from within tasks of the *same* model.

```ez
model BankAccount {
    hidden balance = 0

    init(initial) {
        self.balance = initial
    }

    shown task deposit(amount) {
        self.balance = self.balance + amount
    }

    shown task getBalance() {
        give self.balance
    }
}

acc = new BankAccount(500)
// acc.balance = 1000 // ERROR! Cannot access hidden property from outside!
acc.deposit(100)
out acc.getBalance()
```

## 3. Inheritance (`extends`) and `super`
Models can inherit behavior from a parent model. A child model inherits all `shown` properties and tasks. 

```ez
model Shape {
    init(color) {
        self.color = color
    }
    
    task draw() {
        out "Drawing a " + self.color + " shape."
    }
}

model Circle extends Shape {
    init(color, radius) {
        super(color) // MUST BE CALLED! Initializes the parent state.
        self.radius = radius
    }

    // Overriding the parent method
    task draw() {
        out "Drawing a " + self.color + " circle of radius " + self.radius
    }
}

c = new Circle("red", 5)
c.draw()
```

## 4. Static Tasks
Static tasks belong to the model itself, not to instances. You cannot use `self` inside a static task.
```ez
model MathUtil {
    static task calculateHypotenuse(a, b) {
        give sqrt((a * a) + (b * b))
    }
}

out MathUtil.calculateHypotenuse(3, 4) // 5
```

## 5. Edge Cases & Pitfalls
- **Missing `super()` Call**: If a parent model has an `init` method, the child model's `init` **must** call `super()`. Failing to do so will result in an uninitialized instance and crash the VM.
- **Double `super()` Call**: Calling `super()` twice in the same constructor throws an exception.
- **Hidden Inheritance Visibility**: Properties marked `hidden` in a parent model are completely inaccessible to the child model. The child cannot directly read or overwrite a parent's hidden properties.
- **Accessing Methods as Closures**: You cannot currently detach a method from its object instance and pass it around safely. Calling `detatchedMethod = obj.myTask` followed by `detatchedMethod()` will crash because `self` is no longer bound correctly. Always wrap it: `task wrapper() { obj.myTask() }`.
- **Property Initialization Order**: Class-level property declarations (e.g., `hidden count = 0`) are evaluated *before* the `init` block is run.
