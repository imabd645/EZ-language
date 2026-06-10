# Variables and Data Types

EZ is dynamically typed. Variables are created automatically when you assign a value to them.

## Variables
```ez
name = "Alice"
age = 30
isDeveloper = true
```

## Data Types

EZ supports several primitive and composite data types:

1. **Numbers**: Both Integers and Floats are supported.
   ```ez
   count = 42
   pi = 3.14159
   ```

2. **Strings**: Enclosed in double quotes.
   ```ez
   greeting = "Hello, " + name
   ```

3. **Booleans**: `true`, `false`, `yes`, `no`.
   ```ez
   isReady = yes
   isDone = false
   ```

4. **Nil**: Represents the absence of a value.
   ```ez
   emptyValue = nil
   ```

5. **Arrays**: Ordered collections of values.
   ```ez
   numbers = [1, 2, 3, 4]
   numbers[] = 5  // Append
   ```

6. **Dictionaries**: Key-value collections.
   ```ez
   person = {
       "name": "Bob",
       "age": 25
   }
   person["job"] = "Engineer"
   ```
