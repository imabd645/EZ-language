# 📦 Standard Library & Builtins

EZ includes a comprehensive, cross-platform standard library accessible via built-in packages in `lib/` and the `ezlib` ecosystem.

---

## 1. 💻 The `os` Library Package

Import with `use "os" as *` (or `use "os"` to access submodules). Automatically adapts to Windows, Linux, and macOS without colliding with the built-in `os()` function:

```ez
use "os" as *

// Builtin os() returns host name directly
out os()              // "windows" | "linux" | "macos"

// Platform detection
out getPlatform()     // "windows" | "linux" | "macos"
out isWindows()       // true/false
out isLinux()         // true/false
out isMacOS()         // true/false

// Hardware & System
out "CPU Cores: " + str(cpuCores())
out "Arch: " + arch()          // "x64" | "arm64" | "x86"
out "PID: " + str(pid())
out "Hostname: " + hostname()
out "User: " + userName()
out "Total RAM: " + str(totalMemory())

// Known Directories
out "Home: " + homedir()       // C:\Users\user or /home/user
out "Temp: " + tmpdir()        // C:\Users\...\Temp or /tmp
out "Config: " + dirs.config() // AppData\Roaming or ~/.config

// Environment Variables
env.set("PORT", "8080")
out env.get("PORT")            // "8080"
out env.has("PORT")            // true
env.unset("PORT")

// Path Manipulation
p = path.join("src", "main.ez")
out path.extname("file.tar.gz") // ".gz"
out path.stem("file.tar.gz")    // "file.tar"

// Subprocess Execution
result = process.run("echo hello", { "check": false })
out result["stdout"]            // "hello"

// Unified sys instance
out sys.cwd()
out sys.platform()
```

---

## 2. 📁 The `fs` Package (Native File System)

Import with `use "fs"`:

```ez
use "fs"

// Directory operations
fs.mkdir("my_folder")
out fs.exists("my_folder")        // true
out fs.isDir("my_folder")         // true

// Listing files
files = fs.listDir(".")
for file in files {
    out "File: " + file
}

// Copy, move, delete
fs.copyFile("source.txt", "backup.txt")
fs.deleteFile("backup.txt")
```

---

## 3. 🔐 The `crypto` Package

Import with `use "crypto"`:

```ez
use "crypto"

// Hashing
hash = crypto.sha256("password123")
out "SHA256: " + hash

// Random UUID & bytes
uuid = crypto.uuid4()
out "UUID: " + uuid
```
