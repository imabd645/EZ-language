# CONTRIBUTING.md - Contributing to EZ Language

Thank you for your interest in contributing to the EZ language interpreter! This document provides guidelines for contributing to the project.

## Table of Contents

- [Getting Started](#getting-started)
- [Development Workflow](#development-workflow)
- [Code Style Guidelines](#code-style-guidelines)
- [Testing](#testing)
- [Adding New Builtins](#adding-new-builtins)
- [Documentation](#documentation)
- [Submitting Changes](#submitting-changes)
- [Platform Considerations](#platform-considerations)

## Getting Started

### Prerequisites

- Windows 10 or later, Linux, or macOS
- MinGW-w64 with g++ (MSYS2 recommended)
- CMake 3.10+ (optional)
- Git

### Building the Project

See [BUILD.md](BUILD.md) for detailed build instructions.

**Quick start:**
```bash
git clone https://github.com/imabd645/EZ-language.git
cd EZ-language
build.bat
```

### Running Tests

```bash
ez.exe Test/test.ez
```

## Development Workflow

1. **Fork and clone** the repository
2. **Create a branch** for your feature or bugfix
3. **Make your changes** following the guidelines below
4. **Test thoroughly** (see [Testing](#testing))
5. **Update documentation** if needed
6. **Commit** with clear messages
7. **Push** to your fork
8. **Create a pull request** using the provided [PR template](.github/PULL_REQUEST_TEMPLATE.md)

All contributors are expected to follow the [Code of Conduct](CODE_OF_CONDUCT.md).

If you're filing a bug or requesting a feature, please use the
[issue templates](.github/ISSUE_TEMPLATE/) — they help make sure reports include
the details needed to act on them (affected component, repro steps, environment).

**Security vulnerabilities are the one exception to "open an issue/PR":** do not
file those publicly. See [SECURITY.md](SECURITY.md) for how to report them
privately.

If you're proposing a larger feature or architectural change, check
[ROADMAP.md](ROADMAP.md) first — it lists what's planned, in progress, or
already explored and deliberately deferred (e.g. JIT compilation), so you don't
duplicate effort on something already ruled out.

## Code Style Guidelines

### C++ Code

**Naming Conventions:**
- Files: PascalCase (e.g., `BytecodeVM.h`, `Runtime.cpp`)
- Classes: PascalCase (e.g., `BytecodeVM`, `EZClass`)
- Methods: camelCase (e.g., `defineGlobal`, `runtimeError`)
- Member variables: camelCase (e.g., `stackTop`, `globalEnv`)
- Constants: UPPER_SNAKE_CASE (convention)

**Formatting:**
- Use 4-space indentation
- Opening braces on same line (K&R style)
- Spaces around operators
- No trailing whitespace

**Example:**
```cpp
class BytecodeVM : public RuntimeContext {
public:
    void run(size_t targetFrameCount = 0);
    
private:
    Value* stackTop;
    std::vector<CallFrame> frames;
};
```

### EZ Language Code

**Naming Conventions:**
- Variables: camelCase
- Functions: camelCase
- Classes: PascalCase
- Constants: UPPER_SNAKE_CASE (convention)

**Example:**
```ez
myVariable = 42
myFunction = || {
    return myVariable * 2
}

MyClass = class {
    init(value) {
        this.value = value
    }
}
```

### Comments

**C++:**
- Use `//` for single-line comments
- Use `/* */` for multi-line comments
- Document non-obvious logic
- Reference related code or issues

**EZ:**
- Use `#` for single-line comments
- Document complex algorithms
- Explain workarounds or known issues

## Testing

### Test Organization

**Test/ directory structure:**
- Root: Integration tests and feature demos
- `HandwrittenTests/`: Minimal unit tests for specific features
- Naming convention: `category_number_description.ez`

### Writing Tests

**Location:**
- Feature tests: `Test/`
- Unit tests: `Test/HandwrittenTests/`
- Examples: `examples/`

**Naming:**
- Use descriptive names: `test_new_feature.ez`
- For handwritten: `category_XX_description.ez` (increment XX)

**Test Structure:**
```ez
// test_new_feature.ez
out "=== Testing New Feature ==="

// Test case 1
result = newBuiltin(arg1, arg2)
out "Result: " + str(result)

// Test case 2
try {
    newBuiltin(invalid_arg)
    out "FAIL: Should have thrown error"
} catch e {
    out "PASS: Correctly threw error"
}

out "=== Tests Complete ==="
```

### Running Tests

**Full test suite:**
```bash
ez.exe Test/test.ez
```

**Specific test:**
```bash
ez.exe Test/test_buffer.ez
```

**Handwritten tests:**
```bash
ez.exe Test/HandwrittenTests/arr_51_empty_array.ez
```

### Test Coverage

- Test both success and error cases
- Test edge cases (empty inputs, boundary values)
- Test concurrency-related features with multiple threads
- Verify no memory leaks (run multiple times)

## Adding New Builtins

### Step-by-Step Guide

**1. Choose the appropriate file:**
- String operations → `Builtins_String.cpp`
- Math operations → `Builtins_Math.cpp`
- New subsystem → Create new `Builtins_Subsystem.cpp`

**2. Add the registration:**
```cpp
interp.defineGlobal("functionName", Value::makeNativeFunction("functionName", arity,
    [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
        // Type checking
        if (!args[0].isString()) {
            interp.runtimeError("functionName() expects string as first argument", 0, "");
            return Value();
        }
        
        // Implementation
        std::string s = args[0].asString();
        // ... do work ...
        
        // Return value
        return Value(result);
    }));
```

**3. Register the subsystem in main builtin registration:**
```cpp
// In Builtins.cpp, inside registerBuiltins()
void registerBuiltins(RuntimeContext& interp) {
    registerIOBuiltins(interp);
    registerNetBuiltins(interp);
    registerMathBuiltins(interp);
    registerStringBuiltins(interp);
    registerYourBuiltins(interp);  // Add this
}
```

**4. Add to BUILTINS.md:**
- Document the function signature
- Add example usage
- Mark confidence tier (Confirmed/Inferred)
- Add source file reference

**5. Write tests:**
- Create test file in `Test/` or `Test/HandwrittenTests/`
- Use the exact syntax documented in BUILTINS.md
- Test both success and error cases

**6. Run tests:**
```bash
ez.exe test_new_feature.ez
```

### Variadic Functions

For functions with variable argument counts:

```cpp
interp.defineGlobal("format", Value::makeNativeFunction("format", -1,
    [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
        if (args.empty()) {
            interp.runtimeError("format() expects at least 1 argument", 0, "");
            return Value();
        }
        
        std::string format = args[0].asString();
        // Process remaining args...
        return Value(result);
    }));
```

### Class-Based Builtins

For classes like `Atomic` or `Channel`:

```cpp
// Define class
auto myClass = std::make_shared<EZClass>("MyClass");
CycleCollector::instance().track(myClass, ValueType::CLASS);

// Add methods
myClass->setMethod("init", Value::makeNativeFunction("init", 1,
    [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
        auto instance = args[0].asInstance();
        // Initialize instance properties
        instance->setProperty("_internal", Value(/* ... */));
        return args[0];
    }));

// Register globally
interp.defineGlobal("MyClass", Value(myClass));
```

## Documentation

### Updating Documentation

**When to update:**
- Adding new builtins → Update `BUILTINS.md`
- Changing architecture → Update `ARCHITECTURE.md`
- Changing build process → Update `BUILD.md`
- Changing conventions → Update `AGENTS.md`
- Any user-facing change → Add an entry under `[Unreleased]` in `CHANGELOG.md`

### BUILTINS.md Guidelines

**CRITICAL**: When documenting new builtins, you MUST follow the exact syntax used in actual `.ez` files.

**Rules:**
1. **Tier 1 (Confirmed) examples take precedence** - Use the exact syntax from real `.ez` files
2. **Tier 2 (Inferred) examples** - If no confirmed example exists, use the inferred syntax from C++ registration
3. **Do not improvise** - Never guess argument order, types, or return value handling

**Entry format:**
```markdown
### functionName

**Signature**: `functionName(param1: type, param2: type) -> returnType`

**Return**: Description

**Tier**: Confirmed/Inferred

**Example**:
```ez
result = functionName(arg1, arg2)
```

**Source**: `src/builtins/Builtins_Subsystem.cpp`
```

### ARCHITECTURE.md Guidelines

- Keep implementation details accurate
- Reference specific source files
- Update when architecture changes
- Maintain section organization

### BUILD.md Guidelines

- Update when build process changes
- Note platform-specific requirements
- Keep troubleshooting section current

## Submitting Changes

### Commit Messages

**Format:**
```
<type>: <subject>

<body>
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (no logic change)
- `refactor`: Code refactoring
- `test`: Test additions/changes
- `chore`: Build process or tool changes

**Example:**
```
feat: add reverseString builtin

Adds reverseString() function to Builtins_String.cpp.
Reverses a string using std::reverse.
```

### Pull Request Process

1. **Update documentation** if needed
2. **Add tests** for new functionality
3. **Run full test suite** to ensure no regressions
4. **Update `CHANGELOG.md`** under the `[Unreleased]` section
5. **Create PR** using the [PR template](.github/PULL_REQUEST_TEMPLATE.md), with a clear description
6. **Reference related issues**

Note: as the sole maintainer is listed in [CODEOWNERS](.github/CODEOWNERS), you'll
automatically get a review request on your PR — no need to ping separately.

### Code Review

- Address reviewer feedback promptly
- Keep discussions focused and constructive
- Be open to alternative approaches
- Explain non-obvious decisions

## Platform Considerations

### Cross-Platform Development

The EZ interpreter core is fully cross-platform (GUI is Windows-only). When adding features:

**Consider:**
- Is this feature Windows-specific?
- Can it be made cross-platform?
- If Windows-only, use `#ifdef _WIN32` guards
- Document platform limitations

**Platform-specific code:**
```cpp
#ifdef _WIN32
    // Windows-specific implementation
#else
    // Placeholder for future Unix support
    return Value();
#endif
```

### FFI Safety

When working with FFI code:

**Always use `SAFE_MEMORY_OP` macro on Windows:**
```cpp
SAFE_MEMORY_OP(interp, val = *(uint64_t*)(base + offset));
```

**This prevents access violations from crashing the interpreter.**

If you believe you've found an FFI bug with actual security impact (memory
corruption, exploitable crash, etc.) rather than an ordinary bug, please follow
[SECURITY.md](SECURITY.md) instead of filing a public issue or PR.

### Threading Safety

**VM Thread Affinity:**
- Each `BytecodeVM` instance must run on its owning thread
- Cross-thread calls must marshal through `EventLoop::instance().pushTask()`
- Never share VM-local state across threads

**Shared State:**
- Global Environment is shared across threads (use mutex if needed)
- Arrays, Dicts, Instances use `shared_ptr` and are safe to share
- Use `Mutex` class for explicit synchronization

### Memory Management

**Cycle Collection:**
- Call `CycleCollector::instance().track(ptr, type)` on construction
- Only needed for container objects (arrays, dicts, instances, classes)
- Primitives and simple values don't need tracking

**String Concatenation:**
- Use `EZConcatString` for building large strings
- Avoid manual concatenation in loops

## Known Issues and Limitations

### Current Limitations

- **Cross-platform**: Support for Windows, Linux, and macOS (GUI is Windows-only)
- **No JIT**: Interpreted bytecode only
- **Single-threaded VM**: Each VM instance must run on its thread
- **Manual GC**: Cycle collection is automatic but can be disabled
- **Large executable**: ~17MB due to static linking
- **Hardcoded paths**: Ezlib path is platform-specific

### Areas Needing Work

- Cross-platform support (Linux/macOS)
- JIT compilation for performance
- Generational garbage collection
- Dynamic library loading for ezlib modules
- Better error messages
- More comprehensive test coverage

See [ROADMAP.md](ROADMAP.md) for more detail on which of these are actively
planned versus explored-and-deferred.

## Getting Help

### Resources

- [ARCHITECTURE.md](ARCHITECTURE.md) - System architecture
- [AGENTS.md](AGENTS.md) - AI agent guidelines
- [BUILTINS.md](BUILTINS.md) - Builtin function catalog
- [BUILD.md](BUILD.md) - Build instructions
- [ROADMAP.md](ROADMAP.md) - Planned, in-progress, and deferred work
- [SECURITY.md](SECURITY.md) - How to report vulnerabilities
- [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) - Community standards
- `docs/` directory - Language documentation

### Asking Questions

- Check existing documentation first
- Search for similar issues in the repository
- Provide context: what you're trying to do, what you've tried, what error you're seeing

## License

By contributing to this project, you agree that your contributions will be
licensed under the [MIT License](LICENSE), the same license that covers the
rest of the EZ Language project.

## Thank You

Contributions of any size are welcome! Whether it's a bug fix, new feature, documentation improvement, or test case, it helps make EZ better for everyone.
