# Functions (Tasks)

In EZ, functions are defined using the `task` keyword. Values are returned using the `give` keyword.

## Defining a Task
```ez
task greet(name) {
    out "Hello, " + name + "!"
}

greet("World")
```

## Returning Values
```ez
task add(a, b) {
    give a + b
}

result = add(5, 10)
```

## Default Arguments
Tasks can have optional parameters with default values.
```ez
task multiply(a, b = 2) {
    give a * b
}

out multiply(5)    // Outputs 10
out multiply(5, 3) // Outputs 15
```

## Closures
Tasks can capture variables from their surrounding scope.
```ez
task makeCounter() {
    count = 0
    task counter() {
        count = count + 1
        give count
    }
    give counter
}

c = makeCounter()
out c() // 1
out c() // 2
```
