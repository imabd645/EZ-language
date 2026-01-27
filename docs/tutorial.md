# EZ Language Tutorial

Welcome to EZ! This tutorial will guide you through writing your first programs.

## 1. Hello World
Create a file named `hello.ez` and add:

```javascript
out "Hello, World!"
```

Run it: `./ez hello.ez`

## 2. Variables & Logic
Declared implicitly by assignment.

```javascript
name = "User"
age = 20

when age >= 18 {
    out name + " is an adult."
} other {
    out name + " is a minor."
}
```

## 3. Tasks (Functions)
Defined with `task`. Use `give` to return values.

```javascript
task greet(name) {
    give "Hello " + name
}

out greet("Alice")
```

## 4. Loops
Use `repeat` for counting.

```javascript
repeat i = 1 to 3 {
    out "Count: " + str(i)
}
```

## 5. Working with Data
Arrays and Dictionaries are first-class citizens.

```javascript
todos = ["Code", "Sleep"]
push(todos, "Eat")

get item in todos {
    out "- " + item
}
```

## 6. Asynchronous Tasks
Run heavy operations without blocking.

```javascript
task difficultMath() {
    stop(1000) # Simulating work
    give 42
}

out "Starting..."
future = spawn(difficultMath)
out "Working..."
result = await(future)
out "Result: " + str(result)
```
