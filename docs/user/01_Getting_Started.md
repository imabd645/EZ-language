# 🚀 Getting Started with EZ

Welcome to the **EZ Programming Language**! EZ is an expressive, high-performance programming language designed for rapid application development, seamless native concurrency, Win32 GUI apps, and effortless C interoperability.

---

## 1. 📥 Running EZ Programs

### Execute a Script
To run an `.ez` script, pass the filename to the `ez.exe` executable:
```bash
ez.exe my_script.ez
```

### Interactive REPL Mode
Running `ez.exe` without arguments opens the interactive Read-Eval-Print Loop:
```bash
ez.exe
```
Inside the REPL:
```ez
>> name = "World"
>> out "Hello, " + name + "!"
Hello, World!
```

### Trace & Disassembly Execution
To view runtime bytecode execution traces or disassemble a file:
```bash
# Run with bytecode trace output
ez.exe -trace my_script.ez

# Disassemble compiled bytecode to human-readable .ezb file
ez.exe my_script.ez --dump
```

---

## 2. 📝 Hello World Example

Create a file named `hello.ez`:

```ez
// Print a simple message to stdout
out "Hello, EZ Language!"

// Simple calculation
radius = 5.0
area = 3.14159 * radius ** 2
out "Circle area: " + str(area)
```

Run it:
```bash
ez.exe hello.ez
```

---

## 3. 🛠️ Command-Line Flags

| Option | Description |
| :--- | :--- |
| `ez.exe <file.ez>` | Compiles and executes the specified EZ source file. |
| `ez.exe` | Launches the interactive REPL. |
| `ez.exe -trace <file.ez>` | Executes with step-by-step bytecode execution tracing. |
| `ez.exe <file.ez> --dump` | Disassembles compiled bytecode to `<file>.ezb`. |
| `ez.exe --version` | Prints interpreter version and build configuration. |
