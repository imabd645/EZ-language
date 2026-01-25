# EZ Programming Language

EZ is a modern, high-level, interpreted programming language designed for simplicity, power, and high-performance integration. It features a professional object-oriented model system, a built-in package manager, and native support for SQLite and Web Servers.

## Key Features

- **Object-Oriented**: Support for `model` definitions with constructors and member tasks.
- **Embedded Database**: First-class support for SQLite with parameterized queries.
- **Package Management**: Built-in `ez install` command to fetch libraries from GitHub.
- **Web Capabilities**: Easy-to-use web server built-ins for rapid API development.
- **Modern Syntax**: Expressive control flow (`when`, `repeat`, `get`) and clean syntax.

## Getting Started

### Prerequisites

You need a C++ compiler supporting C++17 (e.g., GCC, Clang, or MSVC) and the following libraries:
- SQLite3
- WinSock2 (on Windows)
- Curl (for package management)

### Compilation

To compile the `ez` interpreter:

```bash
g++ -std=c++17 -Wall -o ez.exe src/main.cpp src/Lexer.cpp src/Parser.cpp src/Interpreter.cpp src/Builtins.cpp -I src -lws2_32 -lsqlite3
```

### Add to PATH (Windows)

To use the `ez` command from any directory:
1. Move `ez.exe` and the required `.dll` files to a permanent folder (e.g., `C:\ezlang`).
2. Open the Start Search, type in "env", and choose "Edit the system environment variables".
3. Click the "Environment Variables" button.
4. Under "System Variables", find the `Path` variable and select "Edit".
5. Click "New" and add the path to your folder (e.g., `C:\ezlang`).
6. Restart your terminal.

## Usage

### Running a Script

To run an EZ script:

```bash
ez path/to/script.ez
```

### Package Management

EZ includes a built-in package manager to install standard libraries and community packages:

```bash
# Install the math library
ez install math

# Install the collections library
ez install collections
```

Packages are stored globally in `C:/ezlib`, making them accessible to all your projects.

## Documentation

For a comprehensive guide to the language syntax and features, see [instructions.md](instructions.md).

## License

This project is licensed under the MIT License - see the LICENSE file for details.
