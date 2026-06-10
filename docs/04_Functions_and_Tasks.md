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

## 3. Deep Closures and State Management
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
- **Variable Shadowing**: If an inner task defines a variable with the same name as a variable in the outer scope, the inner variable shadows the outer one. Modifying the inner variable does not affect the outer scope.
- **Recursive Calls & Stack Overflow**: EZ enforces a strict maximum recursion depth of 10,000 frames to prevent OS-level stack overflows. Exceeding this triggers a fatal VM crash.
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
