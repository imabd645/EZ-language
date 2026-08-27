# 📦 Standard Library & Builtins

EZ includes a comprehensive, cross-platform standard library accessible via built-in packages in `lib/` and the `ezlib` ecosystem.

---

## 1. 💻 The `os` Package (Cross-Platform)

Import with `use "os"`. Automatically adapts to Windows, Linux, and macOS:

```ez
use "os"

// Platform detection
out os.platform()     // "windows" | "linux" | "macos"
out os.isWindows()    // true/false
out os.isLinux()      // true/false
out os.isMacOS()      // true/false

// Hardware & System
out "CPU Cores: " + str(os.cpuCores())
out "Arch: " + os.arch()          // "x64" | "arm64" | "x86"
out "PID: " + str(os.pid())
out "Hostname: " + os.hostname()
out "User: " + os.userName()
out "Total RAM: " + str(os.totalMemory())

// Known Directories
out "Home: " + os.homedir()       // C:\Users\user or /home/user
out "Temp: " + os.tmpdir()        // C:\Users\...\Temp or /tmp
out "Config: " + os.dirs.config() // AppData\Roaming or ~/.config

// Environment Variables
os.env.set("PORT", "8080")
out os.env.get("PORT")            // "8080"
out os.env.has("PORT")            // true
os.env.unset("PORT")

// Path Manipulation
p = os.path.join("src", "main.ez")
out os.path.extname("file.tar.gz") // ".gz"
out os.path.stem("file.tar.gz")    // "file.tar"

// Subprocess Execution
result = os.process.run("echo hello", { "check": false })
out result["stdout"]               // "hello"
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
