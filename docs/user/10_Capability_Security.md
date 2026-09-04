# 🛡️ Capability Security & Safe Execution Mode

EZ includes a capability-based security sandbox designed to safely inspect and run untrusted or third-party scripts.

---

## 1. Running in Safe Mode (`--safe`)

By default, scripts executed with `--safe` run in an isolated environment with restricted access:

- **Native FFI (`os_load_lib`, `FFI.*`)**: Denied.
- **Process Execution (`system`, `exec`)**: Denied.
- **Network Access (`http_get`, `fetch`, sockets)**: Denied.
- **File System Writes (`writeFile`, `appendFile`, `File("w")`)**: Denied.
- **File System Reads (`readFile`, `File("r")`)**: Restricted to current working directory (`.`) and standard library packages.

```powershell
ez script.ez --safe
```

Attempting an unauthorized action will raise a structured `PermissionError` (or prompt interactively if in a terminal).

---

## 2. Granular Capability Flags

You can selectively grant capabilities using explicit command-line flags:

| Flag | Description |
| :--- | :--- |
| `--safe` | Enables capability enforcement sandbox |
| `--allow-all`, `-A` | Bypasses all capability restrictions |
| `--allow-ffi` | Grants native FFI library loading |
| `--allow-process`, `--allow-run` | Grants child process execution (`system()`) |
| `--allow-net` | Grants unrestricted network access |
| `--allow-net=<host>` | Restricts network access to a specific host (e.g. `api.github.com`) |
| `--allow-read` | Grants unrestricted file read access |
| `--allow-read=<path>` | Restricts file read access to a specific path/directory |
| `--allow-write` | Grants unrestricted file write access |
| `--allow-write=<path>` | Restricts file write access to a specific path/directory |
| `--no-prompt` | Disables interactive permission prompts in safe mode (fails immediately on denial) |

### Example

```powershell
ez app.ez --safe --allow-net=api.github.com --allow-read=./config.json --allow-write=./logs
```

---

## 3. Static Permission Analysis (`ez permissions`)

Before executing an unknown script or dependency, use the static analyzer to inspect its AST and imported modules:

```powershell
ez permissions app.ez
```

### Sample Output

```text
=======================================================
 Permissions Analysis: app.ez
=======================================================
 Scanned 2 file(s), 1 package manifest(s).

--- Imported Packages ---
  * http (v1.2.0) - Declared permissions: [net]

--- Detected Capabilities (3) ---
  [READ] readFile (config.json)
      at app.ez:4
  [NET] http_get (https://api.github.com/users)
      at app.ez:12
  [WRITE] writeFile (output.log)
      at app.ez:25

--- Summary of Required Grants ---
  --allow-net=api.github.com
  --allow-read=config.json
  --allow-write=output.log

--- Suggested Execution Command ---
  ez app.ez --safe --allow-net=api.github.com --allow-read=config.json --allow-write=output.log
```

---

## 4. Package Manifest Permissions (`package.ez`)

Package authors can declare the capabilities their library requires in `package.ez`:

```json
{
  "name": "my-pkg",
  "version": "1.0.0",
  "main": "main.ez",
  "permissions": [
    "net",
    "read"
  ],
  "dependencies": {}
}
```

When inspecting packages via `ez info <pkg>`, EZ displays declared capabilities to help users evaluate trust before installing or running.

---

## 5. Interactive Permission Prompting

When executing in `--safe` mode within an interactive terminal, if an ungranted capability is requested, EZ prompts:

```text
[Security Prompt] Script requested capability: NET ('https://api.github.com')
Allow access? [y]es / [n]o / [a]lways: 
```

- **`y` / `yes`**: Grants access for that specific call only.
- **`a` / `always`**: Grants access and adds the resource to the session whitelist for subsequent calls without further prompting.
- **`n` / `no` / Enter**: Denies access and raises `PermissionError`.

For automated CI/CD pipelines, pass `--no-prompt` to ensure scripts fail immediately if required flags are missing.
