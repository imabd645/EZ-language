# EZ Language

<p align="center">
  <img src="https://img.shields.io/badge/Version-1.0.0-purple?style=for-the-badge" alt="Version">
  <img src="https://img.shields.io/badge/License-MIT-blue?style=for-the-badge" alt="License">
  <img src="https://img.shields.io/badge/Platform-Windows-cyan?style=for-the-badge" alt="Platform">
</p>

<p align="center">
  <b>Simple. Intuitive. Powerful.</b><br>
  A beginner-friendly programming language with human-readable syntax
</p>

---

## ✨ Features

- **Intuitive Keywords** - Use natural words like `when`, `repeat`, `until`, and `task`
- **Easy to Learn** - Get productive in minutes with simple, readable syntax
- **VS Code Support** - Full syntax highlighting and code snippets
- **No Semicolons** - Focus on logic, not punctuation
- **Readable Booleans** - Use `yes` and `no` instead of `true` and `false`

## 🚀 Quick Start

### Installation

1. **Download** `ez.exe` from the [releases](https://github.com/imabd645/EZ-language/releases)
2. **Create a folder** (e.g., `C:\EZ`) and move `ez.exe` there
3. **Add to PATH**:
   - Press `Win + R`, type `sysdm.cpl`, press Enter
   - Go to **Advanced** → **Environment Variables**
   - Under **System variables**, find **Path**, click **Edit**
   - Click **New** and add: `C:\EZ`
   - Click **OK** to save
4. **Run from anywhere**: `ez yourfile.ez`

### Hello World

Create a file named `hello.ez`:

```
out "Hello, World!"
```

Run it:

```bash
ez hello.ez
```

## 📖 Language Syntax

### Keywords

| EZ Keyword | Traditional | Description |
|------------|-------------|-------------|
| `when` | `if` | Conditional statement |
| `other` | `else` | Else block |
| `repeat` | `for` | For loop |
| `until` | `while` | While loop |
| `task` | `function` | Function definition |
| `give` | `return` | Return value |
| `out` | `print` | Output to console |
| `in` | `input` | Get user input |
| `get` | `foreach` | Iterate over collection |
| `escape` | `break` | Exit loop |
| `skip` | `continue` | Skip to next iteration |
| `yes` / `no` | `true` / `false` | Boolean values |

### Operators

| EZ Operator | Traditional | Description |
|-------------|-------------|-------------|
| `equal` | `==` | Equality check |
| `notequal` | `!=` | Inequality check |
| `greater` | `>` | Greater than |
| `less` | `<` | Less than |
| `greaterthen` | `>=` | Greater than or equal |
| `lessthen` | `<=` | Less than or equal |
| `and` | `&&` | Logical AND |
| `or` | `\|\|` | Logical OR |
| `not` | `!` | Logical NOT |

## 💡 Examples

### Functions

```
task greet(name) {
    out "Hello, " + name + "!"
}

greet("World")
```

### Conditionals

```
task checkAge(age) {
    when age greaterthen 18 {
        out "Adult"
    } other when age equal 18 {
        out "Just turned adult!"
    } other {
        out "Minor"
    }
}
```

### Loops

```
// For loop
repeat i = 1 to 5 {
    out i
}

// While loop
x = 10
until x equal 0 {
    out x
    x = x - 1
}

// Foreach
colors = ["red", "green", "blue"]
get color in colors {
    out color
}
```

### Recursion

```
task fibonacci(n) {
    when n lessthen 2 {
        give n
    }
    give fibonacci(n - 1) + fibonacci(n - 2)
}

repeat i = 0 to 10 {
    out fibonacci(i)
}
```

## 🎨 VS Code Extension

Get syntax highlighting and code snippets for EZ Language in VS Code!

### Installation

1. Download `ez-language.vsix`
2. Open VS Code
3. Press `Ctrl + Shift + X` to open Extensions
4. Click `...` menu → **Install from VSIX...**
5. Select the downloaded file

### Features

- ✅ Syntax highlighting for all keywords
- ✅ 35+ code snippets
- ✅ Auto-completion
- ✅ Comment support

## 📁 Project Structure

```
EZ-language/
├── ez.exe              # EZ interpreter
├── ez-language/        # VS Code extension
│   ├── package.json
│   ├── syntaxes/
│   └── snippets/
├── examples/           # Sample EZ programs
└── README.md
```

## 🤝 Contributing

Contributions are welcome! Feel free to:

- Report bugs
- Suggest new features
- Submit pull requests

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 👨‍💻 Author

**Abdullah Masood**

- GitHub: [@imabd645](https://github.com/imabd645)
- LinkedIn: [imabd645](https://linkedin.com/in/imabd645)
- Email: abdullahmasood645@gmail.com

---

<p align="center">
  Made with ❤️ for beginners everywhere
</p>
