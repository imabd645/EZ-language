# EZ Language - In-Depth Introduction and Architecture

Welcome to the definitive, comprehensive guide to **EZ**. EZ is a statically-compiled yet dynamically-typed scripting language engineered to deliver the syntactic joy of modern high-level languages while maintaining the blistering speed and raw system access of C.

## 1. Architectural Overview
When you write an EZ script, it does not interpret the code line-by-line. Instead, the runtime goes through a rigorous compilation pipeline:
1. **Lexical Analysis (Lexer)**: The source code is tokenized. White space is generally ignored, except newlines which act as implicit statement terminators.
2. **Abstract Syntax Tree (Parser)**: The tokens are parsed into an AST. The parser is strict and enforces scoping rules early on.
3. **Bytecode Compilation (Compiler)**: The AST is lowered into EZ Bytecode (a custom, register-based and stack-based hybrid instruction set).
4. **Execution (BytecodeVM)**: The VM executes the bytecode. It features a Mark-and-Sweep Garbage Collector to handle memory automatically.

## 2. A Comprehensive "Hello World"
Let's look at a script that demonstrates several features at once: variables, tasks, conditionals, and standard output.

```ez
// main.ez
task guessTheNumber(target, guess) {
    when guess == target {
        out "You guessed it! The number was " + target
    } when guess > target {
        out "Too high!"
    } other {
        out "Too low!"
    }
}

secretNumber = 42
myGuess = 50

out "Starting the game..."
guessTheNumber(secretNumber, myGuess)
```

## 3. How to Compile and Run
To execute your EZ scripts, you pass the file to the Windows executable.
```bash
# In PowerShell or Command Prompt
./ez.exe main.ez
```
The VM automatically handles all stages. If a runtime error occurs, EZ prints a Java-style stack trace indicating the exact line number across all imported files where the error originated.

## 4. Syntax Philosophy
- **Natural Language Keywords**: Using `when/other` instead of `if/else`, and `get in` instead of `for..in`.
- **No Semicolons**: Statements end with a newline.
- **Block Scoping**: Variables are lexically scoped within `{}` blocks.

## 5. Edge Cases & Parsing Nuances
- **Implicit Semicolons**: EZ uses newlines to separate statements. If you break a long expression across multiple lines, you MUST ensure the parser knows it's incomplete. For example, leaving a `+` or `,` at the end of the line will continue the expression.
  ```ez
  // Valid: The '+' tells the parser to keep looking
  total = 100 +
          200 +
          300

  // Invalid: This evaluates 'total = 100' and then throws a syntax error on '+ 200'
  total = 100
          + 200
  ```
- **Trailing Commas**: Unlike JavaScript or Python, EZ's parser strictly forbids trailing commas in arrays or dictionaries.
  ```ez
  // Valid
  list = [1, 2, 3]

  // Throws Syntax Error
  list = [1, 2, 3,] 
  ```
- **File Encoding**: The parser expects UTF-8 formatted files. BOMs (Byte Order Marks) may cause syntax errors on the first line.
