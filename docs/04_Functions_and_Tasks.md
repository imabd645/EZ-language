# Functions (Tasks) and Closures

In EZ, functions are declared using the `task` keyword. The `give` keyword is used to return a value.

## 1. Basic Tasks & Recursion
```ez
// A simple recursive task
task fibonacci(n) {
    when n <= 1 {
        give n
    }
    give fibonacci(n - 1) + fibonacci(n - 2)
}

out "Fib(10) is " + fibonacci(10)
```

## 2. Default Parameters
Parameters can have default fallback values evaluated at definition time.
```ez
task createUser(name, role = "Guest", active = yes) {
    give {
        "name": name,
        "role": role,
        "active": active
    }
}

user1 = createUser("Alice") 
// {name: "Alice", role: "Guest", active: true}

user2 = createUser("Bob", "Admin")
```

## 3. Keyword Arguments
When calling tasks, you can pass arguments by name rather than position. This is especially useful for tasks with many default parameters, allowing you to skip parameters you don't want to provide.
```ez
task configureWindow(width=800, height=600, title="App", fullscreen=false) {
    out "Creating " + title + " at " + width + "x" + height
}

// Pass arguments completely out of order!
configureWindow(title="My Game", fullscreen=true)
```

## 4. The Spread & Rest Operators (`...`)
If you have an array of values, you can instantly "unpack" or "spread" them into a function call as separate arguments using the `...` operator.
```ez
task addThreeNumbers(a, b, c) {
    give a + b + c
}

myNumbers = [10, 20, 30]
out addThreeNumbers(...myNumbers) // 60
```

Conversely, you can use the same `...` operator in a task's parameter list to "gather" an infinite number of arguments into a single Array. This is known as a Rest Parameter.
```ez
task sumAll(...numbers) {
    total = 0
    get n in numbers { total = total + n }
    give total
}

out sumAll(1, 2, 3, 4, 5) // 15
```

## 5. Deep Closures and State Management
Tasks in EZ support closures. An inner task can capture variables from an outer task. The VM intelligently detects this and promotes the captured variable from the stack to the heap, ensuring it survives after the outer task finishes.

```ez
task createBank(initialBalance) {
    balance = initialBalance
    
    task deposit(amount) {
        balance = balance + amount
        give balance
    }
    
    task withdraw(amount) {
        when amount > balance {
            out "Insufficient funds!"
            give false
        }
        balance = balance - amount
        give balance
    }
    
    // Return a dictionary of tasks (methods)
    give {
        "deposit": deposit,
        "withdraw": withdraw
    }
}

myAccount = createBank(100)
out myAccount["deposit"](50)  // 150
out myAccount["withdraw"](20) // 130
```

## 4. First-Class Functions (High-Order Tasks)
Tasks can be passed as arguments to other tasks, allowing for functional programming paradigms.
```ez
task mapArray(arr, transformTask) {
    result = []
    get item in arr {
        result[] = transformTask(item)
    }
    give result
}

numbers = [1, 2, 3, 4]

task square(x) { give x * x }

squaredNumbers = mapArray(numbers, square)
out squaredNumbers // [1, 4, 9, 16]
```

## 5. Edge Cases & Pitfalls
- **Recursive Calls & Stack Overflow**: EZ limits the recursion call stack to prevent OS-level stack overflows. Exceeding the call depth limit does *not* crash the host process; it safely aborts the script by throwing a clean traceback error.
- **Loop Closure Binding (By-Reference Capturing)**: When creating closures inside loops (e.g. `repeat`), EZ dynamically captures the loop variable *by reference* (late binding). This means closures share the exact same variable. If you execute them later, they will all see the *final mutated value* of the loop variable, rather than the value it had during their specific iteration.
- **Omitting `give`**: If a task reaches the end of its block without a `give` statement, it implicitly returns `nil`.
- **Default Argument Evaluation**: Default arguments are evaluated *at definition time*, not at call time. If you use a mutable object (like `[]` or `{}`) as a default argument, the *same* object reference will be used for every call!
  ```ez
  // Bad practice:
  task addToList(val, list = []) {
      list[] = val
      give list
  }
  // Subsequent calls will append to the same persistent list!
  ```
  *Best practice is to use `list = nil` and initialize it inside the task body.*
