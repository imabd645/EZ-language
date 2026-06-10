# Object-Oriented Programming

EZ provides a robust class-like system using the `model` keyword.

## Creating Models
Use `model` to define a blueprint. `init` is the constructor. Methods are defined as inner tasks. `self` is used to refer to the instance.

```ez
model Person {
    init(name, age) {
        self.name = name
        self.age = age
    }

    task display() {
        out "Name: " + self.name + ", Age: " + self.age
    }
}

p = new Person("Alice", 30)
p.display()
```

## Access Modifiers (`shown` / `hidden`)
By default, properties and methods are public. You can explicitly mark them:
- `shown`: Publicly accessible (default).
- `hidden`: Private to the model.

```ez
model BankAccount {
    hidden balance = 0

    task deposit(amount) {
        self.balance = self.balance + amount
    }
}
```

## Inheritance (`extends`)
Models can inherit from other models using the `extends` keyword. Use `super()` to call the parent's constructor or methods.

```ez
model Employee extends Person {
    init(name, age, jobTitle) {
        super(name, age)
        self.jobTitle = jobTitle
    }
}
```
