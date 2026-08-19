# fs — File System Library for EZ

> **Version:** 1.0  
> **Import:** `use "fs"`  
> **File:** `C:\ezlib\fs\main.ez`  
> **Requires:** `Kernel32.dll` (Windows, always available)

---

## Overview

`fs` is a native file system library for EZ built entirely on top of the Windows `Kernel32.dll` API through FFI. It exposes a clean `fs` dictionary object with operations for:

- Checking file/directory existence and type
- Creating and removing directories and files
- Copying and moving files
- Listing directory contents
- Getting file size and last modification time
- Reading, writing, and appending file content

---

## Quick Start

```ez
use "fs"

# Check and create directory
when not fs.exists("output") {
    fs.mkdir("output")
}

# Write a file
fs.writeFile("output/hello.txt", "Hello from EZ!")

# Read it back
content = fs.readFile("output/hello.txt")
out content   # → "Hello from EZ!"

# List directory
files = fs.listDir("output")
out files  # → ["hello.txt"]
```

---

## The `fs` Object

After `use "fs"`, a global dictionary named `fs` is available with all file operations as callable values.

---

## API Reference

### `fs.exists(path)` → `boolean`

Returns `true` if the file or directory at `path` exists.

Uses `GetFileAttributesA` — returns `false` if the attribute code is `-1` (INVALID_FILE_ATTRIBUTES).

```ez
use "fs"

out fs.exists("C:\\Windows")            # → true
out fs.exists("C:\\nonexistent")        # → false
out fs.exists("myfile.txt")             # → true / false
```

---

### `fs.isDir(path)` → `boolean`

Returns `true` if the path exists AND is a directory (not a regular file).

Uses `FILE_ATTRIBUTE_DIRECTORY` (bit 4 = 0x10) of the attributes bitmask.

```ez
use "fs"

out fs.isDir("C:\\Windows")        # → true
out fs.isDir("C:\\Windows\\notepad.exe")  # → false
out fs.isDir("nonexistent")        # → false
```

---

### `fs.mkdir(path)` → `boolean`

Creates a new directory at the given path.

Uses `CreateDirectoryA`. Returns `true` if successful, `false` if it already exists or creation fails.

> ⚠️ Creates only **one level** of directory. To create nested directories, create each level manually.

```ez
use "fs"

out fs.mkdir("output")         # → true (created)
out fs.mkdir("output")         # → false (already exists)

# Create nested manually
fs.mkdir("output")
fs.mkdir("output\\logs")
fs.mkdir("output\\logs\\2025")
```

---

### `fs.remove(path)` → `boolean`

Removes a file or an **empty** directory.

- If the path is a directory → calls `RemoveDirectoryA`
- If the path is a file → calls `DeleteFileA`

Returns `false` if the path doesn't exist or removal fails (e.g., directory not empty).

```ez
use "fs"

fs.writeFile("temp.txt", "data")
out fs.remove("temp.txt")     # → true

fs.mkdir("emptydir")
out fs.remove("emptydir")     # → true (empty dir)

# Cannot remove non-empty directories
fs.mkdir("dir")
fs.writeFile("dir\\file.txt", "data")
out fs.remove("dir")          # → false (not empty)
```

---

### `fs.copy(src, dest)` → `boolean`

Copies a file from `src` to `dest`. Overwrites `dest` if it already exists.

Uses `CopyFileA` with `failIfExists = 0`.

```ez
use "fs"

fs.writeFile("original.txt", "Hello!")
out fs.copy("original.txt", "backup.txt")  # → true

out fs.readFile("backup.txt")  # → "Hello!"
```

---

### `fs.move(src, dest)` → `boolean`

Moves (renames) a file from `src` to `dest`.

Uses `MoveFileA`. Returns `true` on success, `false` on failure.

> ⚠️ `MoveFileA` cannot move files across different volumes/drives.

```ez
use "fs"

fs.writeFile("draft.txt", "Work in progress")
out fs.move("draft.txt", "final.txt")  # → true

out fs.exists("draft.txt")  # → false (moved)
out fs.exists("final.txt")  # → true
```

---

### `fs.listDir(path)` → `array`

Lists all files and subdirectories in a directory. Returns their names (not full paths). Excludes `.` and `..`.

Uses `FindFirstFileA` / `FindNextFileA` with pattern `path\*`.

Returns an empty array `[]` if the directory doesn't exist or is inaccessible.

```ez
use "fs"

fs.mkdir("testdir")
fs.writeFile("testdir\\a.txt", "A")
fs.writeFile("testdir\\b.txt", "B")
fs.mkdir("testdir\\subdir")

entries = fs.listDir("testdir")
out entries   # → ["a.txt", "b.txt", "subdir"]
```

---

### `fs.size(path)` → `number`

Returns the size of a file in bytes. Returns `-1` if the file doesn't exist.

Reads `WIN32_FIND_DATAA.nFileSizeHigh` and `nFileSizeLow` and combines them into a 64-bit value.

```ez
use "fs"

fs.writeFile("data.txt", "Hello World!")   # 12 bytes
out fs.size("data.txt")    # → 12

out fs.size("nonexistent.txt")  # → -1
```

---

### `fs.modifiedTime(path)` → `number`

Returns the last-modified time of a file as a Unix millisecond timestamp. Returns `-1` if the file doesn't exist.

Reads `WIN32_FIND_DATAA.ftLastWriteTime` (Windows FILETIME), converts from 100-nanosecond intervals since 1601-01-01 to Unix epoch milliseconds.

```ez
use "datetime"
use "fs"

fs.writeFile("test.txt", "data")
modTime = fs.modifiedTime("test.txt")
out format(modTime)  # → "2025-06-11 14:30:00" (UTC)
```

---

### `fs.readFile(path)` → `string`

Reads the entire content of a text file and returns it as a string.

Wraps the EZ built-in `readFile()`.

```ez
use "fs"

fs.writeFile("config.txt", "key=value\nfoo=bar")
content = fs.readFile("config.txt")
out content
# → "key=value\nfoo=bar"
```

---

### `fs.writeFile(path, content)` → `boolean`

Writes a string to a file, **overwriting** any existing content.

Wraps the EZ built-in `writeFile()`.

```ez
use "fs"

fs.writeFile("log.txt", "First line")
fs.writeFile("log.txt", "Overwritten!")  # Replaces previous content

out fs.readFile("log.txt")  # → "Overwritten!"
```

---

### `fs.appendFile(path, content)` → `boolean`

Appends a string to the end of a file. Creates the file if it doesn't exist.

Wraps the EZ built-in `appendFile()`.

```ez
use "fs"

fs.writeFile("log.txt", "")        # Create empty
fs.appendFile("log.txt", "Line 1\n")
fs.appendFile("log.txt", "Line 2\n")
fs.appendFile("log.txt", "Line 3\n")

out fs.readFile("log.txt")
# → "Line 1\nLine 2\nLine 3\n"
```

---

## Edge Cases & Important Notes

### Windows-Only
`fs` is built exclusively on `Kernel32.dll` and is **Windows-only**. It will not work on Linux or macOS.

### Paths: Backslashes
Windows paths use `\` separators. In EZ strings, use `\\` to escape:
```ez
use "fs"

out fs.exists("C:\\Users\\HP\\Documents")
fs.writeFile("C:\\Temp\\output.txt", "Hello")
```
Forward slashes `/` may also work in some contexts (Windows kernel accepts both), but `Kernel32.dll` functions officially require backslashes.

### mkdir is Non-Recursive
`fs.mkdir()` creates only **one directory level** at a time. Creating `"a\\b\\c"` requires three separate calls:
```ez
use "fs"

fs.mkdir("a")
fs.mkdir("a\\b")
fs.mkdir("a\\b\\c")
```

### remove Only Works on Empty Directories
`RemoveDirectoryA` only succeeds on empty directories. To recursively delete a directory tree, list and delete all contents first:
```ez
use "fs"

task removeDir(path) {
    entries = fs.listDir(path)
    get e in entries {
        fullPath = path + "\\" + e
        when fs.isDir(fullPath) {
            removeDir(fullPath)
        } other {
            fs.remove(fullPath)
        }
    }
    fs.remove(path)
}
```

### Large File Size
`fs.size()` constructs a 64-bit size value. Very large files (>4GB) use both high and low 32-bit parts from the WIN32_FIND_DATAA structure. The bitwise arithmetic handles this correctly.

### modifiedTime is UTC
The converted timestamp from Windows FILETIME is UTC. Add timezone offset using `datetime` if needed.

### File Path Lengths
Windows traditionally limits paths to 260 characters (`MAX_PATH`). Very deep directory trees may cause issues.

---

## Full Example: Directory Scanner

```ez
use "fs"

task scanDirectory(path, depth) {
    indent = ""
    repeat i = 0 to depth - 1 { indent = indent + "  " }
    
    entries = fs.listDir(path)
    get name in entries {
        fullPath = path + "\\" + name
        when fs.isDir(fullPath) {
            out indent + "[DIR] " + name + " (" + str(len(fs.listDir(fullPath))) + " items)"
            scanDirectory(fullPath, depth + 1)
        } other {
            sizeBytes = fs.size(fullPath)
            out indent + "[FILE] " + name + " (" + str(sizeBytes) + " bytes)"
        }
    }
}

out "Scanning C:\\Projects:"
scanDirectory("C:\\Projects", 0)
```

---

## Full Example: Log Rotation

```ez
use "fs"

LOG_FILE = "app.log"
MAX_SIZE = 1048576  # 1 MB

task writeLog(message) {
    # Rotate if too large
    when fs.exists(LOG_FILE) and fs.size(LOG_FILE) > MAX_SIZE {
        fs.move(LOG_FILE, "app.log.old")
        out "Log rotated."
    }
    
    fs.appendFile(LOG_FILE, message + "\n")
}

repeat i = 0 to 99 {
    writeLog("Log entry number " + str(i) + " with some additional data padding.")
}

out "Final log size: " + str(fs.size(LOG_FILE)) + " bytes"
```

---

## Full Example: File Backup Tool

```ez
use "fs"

task backupFiles(sourceDir, backupDir) {
    when not fs.exists(backupDir) {
        fs.mkdir(backupDir)
    }
    
    files = fs.listDir(sourceDir)
    copiedCount = 0
    
    get name in files {
        src = sourceDir + "\\" + name
        dest = backupDir + "\\" + name
        
        when not fs.isDir(src) {
            success = fs.copy(src, dest)
            when success {
                copiedCount = copiedCount + 1
                out "Backed up: " + name + " (" + str(fs.size(src)) + " bytes)"
            } other {
                out "Failed to backup: " + name
            }
        }
    }
    
    out "Backup complete. " + str(copiedCount) + " files copied."
}

backupFiles("C:\\Projects\\myapp", "C:\\Backups\\myapp_backup")
```

---

## Full Example: Config File Manager

```ez
use "fs"

CONFIG_PATH = "config.ini"

task readConfig() {
    when not fs.exists(CONFIG_PATH) {
        give {}
    }
    
    content = fs.readFile(CONFIG_PATH)
    lines = split(content, "\n")
    config = {}
    
    get line in lines {
        line = replace(line, "\r", "")
        when len(line) > 0 and indexOf(line, "=") != -1 {
            parts = split(line, "=")
            key = parts[0]
            value = parts[1]
            config[key] = value
        }
    }
    
    give config
}

task saveConfig(config) {
    content = ""
    get k in config {
        content = content + k + "=" + str(config[k]) + "\n"
    }
    fs.writeFile(CONFIG_PATH, content)
}

# Load or create config
cfg = readConfig()
when not has_key(cfg, "theme") { cfg["theme"] = "dark" }
when not has_key(cfg, "language") { cfg["language"] = "en" }
saveConfig(cfg)

out "Theme: " + cfg["theme"]
out "Language: " + cfg["language"]
```

---

*Documentation generated from `C:\ezlib\fs\main.ez` — EZ File System Library*
