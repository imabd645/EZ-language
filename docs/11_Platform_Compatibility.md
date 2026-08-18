# EZ Language - Platform Compatibility & Windows-Only Features

This document provides a comprehensive breakdown of platform support in the EZ Programming Language, detailing features that are fully cross-platform and subsystems that remain exclusive to Windows.

---

## 1. Platform Support Matrix

| Subsystem / Feature Area | Windows | Linux (Ubuntu / Debian / POSIX) | macOS (Apple Silicon & Intel) |
| :--- | :---: | :---: | :---: |
| **Core Interpreter & Bytecode VM** | ✅ Fully Supported | ✅ Fully Supported | ✅ Fully Supported |
| **Compiler, Lexer, Parser & AST** | ✅ Fully Supported | ✅ Fully Supported | ✅ Fully Supported |
| **Static Type Checker** | ✅ Fully Supported | ✅ Fully Supported | ✅ Fully Supported |
| **Garbage Collector & Cycle Detection** | ✅ Fully Supported | ✅ Fully Supported | ✅ Fully Supported |
| **Async / Await & Event Loop (libuv)** | ✅ Fully Supported | ✅ Fully Supported | ✅ Fully Supported |
| **Threading & Concurrency** (`spawn`, `mutex`, `channel`, `atomic`) | ✅ Fully Supported | ✅ Fully Supported | ✅ Fully Supported |
| **Standard Builtins** (Math, String, Arrays, Dictionaries, JSON, Time/Date) | ✅ Fully Supported | ✅ Fully Supported | ✅ Fully Supported |
| **Streaming File I/O** (`File` class, read/write/seek/tell) | ✅ Fully Supported | ✅ Fully Supported | ✅ Fully Supported |
| **Networking & HTTP Client** (`libcurl`) | ✅ Fully Supported | ✅ Fully Supported | ✅ Fully Supported |
| **SQLite & ORM Engine** (`libsqlite3`) | ✅ Fully Supported | ✅ Fully Supported | ✅ Fully Supported |
| **FFI Core** (`libffi`, Structs, Callbacks, Memory Ops, Crash Guards) | ✅ Fully Supported | ✅ Fully Supported | ✅ Fully Supported |
| **Native GUI Framework** (`gui_*` builtins) | ✅ Supported | ❌ Not Supported | ❌ Not Supported |
| **Executable Packager** (`ez package`) | ✅ Supported | ❌ Not Supported | ❌ Not Supported |
| **Windows Registry API** | ✅ Supported | ❌ Not Supported | ❌ Not Supported |
| **Windows Console Low-Level APIs** | ✅ Supported | ⚠️ Partial (VT100 Fallback) | ⚠️ Partial (VT100 Fallback) |
| **Desktop Notifications** (`test_notify.ez`) | ✅ Supported | ❌ Not Supported | ❌ Not Supported |

---

## 2. Features that are Windows-Only

The following features depend on Windows-specific Win32 APIs, PE executable formats, or Windows system DLLs and will **not** function on Linux or macOS.

### 2.1 Native GUI Framework (`src/gui/`)
The built-in GUI library provides native desktop windowing and controls built directly on the **Win32 API** and **GDI**:
* **Window Management**: `gui_window`, `gui_loop`, `gui_quit`, modal dialogs, and window subclassing (`SetWindowSubclass`, `CreateWindowEx`, `HWND`).
* **Container Widgets**: Panels, Native Tab Controls (`WC_TABCONTROL`), ScrollPanels with mouse-wheel routing, and Sidebars.
* **Standard Controls**:
  - `gui_create_button`, `gui_create_label`, `gui_create_input`, `gui_create_textarea`
  - `gui_create_checkbox`, `gui_create_radio`, `gui_create_slider`, `gui_create_progress`
  - `gui_create_groupbox`, `gui_create_separator`, `gui_create_image`
  - `gui_create_listview` (SysListView32 with custom row/column helpers)
  - `gui_create_treeview` (SysTreeView32 hierarchical trees)
  - `gui_create_datepicker` (SysDateTimePick32)
  - `gui_create_spinner` (msctls_updown32)
* **Theming & Win11 Dark Mode**: Direct calls to `dwmapi.dll` (`DwmSetWindowAttribute`) and `uxtheme.dll` (`SetWindowTheme`) for modern Windows 11 aesthetics and brush caching.
* **Menus**: `gui_create_menubar`, `gui_create_menu`, `gui_menu_add_item`, `gui_menu_add_separator`, and right-click context menus (`TrackPopupMenu`).

> **Note**: On non-Windows platforms, `registerGUIBuiltins` is omitted during compilation. Calling `gui_*` functions on Linux or macOS will result in an "Undefined variable / function" error.

---

### 2.2 Standalone Executable Packager (`ez package`)
The `ez package` CLI command bundles EZ scripts, bytecode, and dependencies into a standalone `.exe`:
* **PE Binary Embedding**: Modifies Windows Portable Executable (PE) headers and appends bytecode payload sections (`.ez_data`) to an embedded `ez.exe` stub runtime.
* **Limitation**: Produces Windows PE executables (`.exe`). It does not produce Linux ELF or macOS Mach-O standalone bundles.

---

### 2.3 Windows Registry Access (`src/cli/Registry.h`)
* Functions interacting with `HKEY_CURRENT_USER` and `HKEY_LOCAL_MACHINE` (e.g., `RegOpenKeyExA`, `RegQueryValueExA`, `RegSetValueExA`) to configure system file associations (`.ez` file extension registration).
* Not applicable on POSIX systems (Linux uses `.desktop` MIME types, macOS uses `Info.plist` LaunchServices).

---

### 2.4 Windows Console APIs (`src/builtins/Builtins_Console.cpp`)
* Low-level console manipulation functions using Windows Console Handles (`GetStdHandle(STD_OUTPUT_HANDLE)`):
  - `console_cursor_pos(x, y)`: `SetConsoleCursorPosition`
  - `console_hide_cursor()`: `SetConsoleCursorInfo`
  - `console_title(title)`: `SetConsoleTitleA`
  - `console_size()`: `GetConsoleScreenBufferInfo`
  - `console_read_key()`: Uses `_kbhit()` and `_getch()` from `<conio.h>` for non-blocking raw key capture.
* **POSIX Status**: Standard ANSI/VT100 escape codes handle color and basic clearing on Linux/macOS, but Windows-specific handle calls are skipped.

---

### 2.5 Windows Desktop Notifications (`test_notify.ez`)
* Desktop tray balloon notifications using `Shell_NotifyIconA` and `NOTIFYICONDATA` from `shell32.dll`.
* Linux uses `libnotify` / `notify-send` and macOS uses `osascript` / `NSUserNotification`, which are not currently wired to `test_notify.ez`.

---

### 2.6 Windows System DLL Calls in FFI Scripts
Scripts that make direct FFI calls (`os_load_lib`, `os_bind_sym`) targeting Windows DLLs will fail on non-Windows platforms:
* `Kernel32.dll` (e.g., `Beep`, `GetTickCount`, `GetCurrentProcessId`)
* `User32.dll` (e.g., `MessageBoxA`, `EnumWindows`, `GetSystemMetrics`)
* `msvcrt.dll` (e.g., `puts`, `sin`, `cos` — use standard POSIX `libc` on Linux/macOS instead)
* `gdi32.dll`, `dwmapi.dll`, `comctl32.dll`

---

## 3. Feasibility Analysis: Porting Remaining Features to POSIX

Other than the native Win32 GUI subsystem, all other Windows-specific features can be ported to Linux and macOS with relatively minimal effort:

```
┌──────────────────────────────────────┬────────────────────────┬──────────────────────────────────────────┐
│ Feature                              │ Porting Difficulty     │ Cross-Platform Implementation Strategy   │
├──────────────────────────────────────┼────────────────────────┼──────────────────────────────────────────┤
│ 1. Console Utilities & Colors        │ 🟢 Trivial (Near 100%) │ Pure ANSI/VT100 & termios (already live) │
│ 2. Desktop Notifications (notify.ez) │ 🟢 Easy                │ notify-send (Linux) / osascript (macOS)  │
│ 3. Executable Packager (ez package)  │ 🟡 Moderate            │ Binary append + chmod +x (ELF & Mach-O)  │
│ 4. System File Associations          │ 🟢 Easy                │ XDG Desktop Entry (Linux) / duti (macOS) │
│ 5. Native GUI Framework              │ 🔴 Complex             │ Requires Webview / SDL / Skia / Qt / GTK │
└──────────────────────────────────────┴────────────────────────┴──────────────────────────────────────────┘
```

### 3.1 Console Utilities (`Builtins_Console.cpp`) — *Near Complete*
* **Current Status**: `clear`, `color`, `reset`, `gotoxy`, and `getch` already have built-in POSIX/VT100 and `termios` fallback implementations.
* **Remaining**: Window resizing query using `ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)` and setting terminal titles via ANSI OSC `\033]0;Title\007`.

### 3.2 Desktop Notifications (`lib/notify.ez`) — *Easy*
* `lib/notify.ez` is written purely in user-space EZ code using dynamic FFI.
* **Linux**: Invoke `notify-send` via standard system process or bind `libnotify.so`.
* **macOS**: Invoke `osascript -e 'display notification ...'` or bind `NSUserNotificationCenter` via `libobjc.dylib`.

### 3.3 Standalone Executable Packager (`src/cli/Packager.cpp`) — *Moderate*
* The core packager already uses a platform-neutral payload format: it concatenates the base runtime binary with serialized bytecode using the trailing footer `EZ_MAGIC_PACK`.
* **Linux / macOS**: Copy the local `ez` binary, append the bytecode payload, and set executable permissions (`chmod +x` / `0755`). Only Windows `.ico` icon injection (`BeginUpdateResourceA`) is Windows-specific.

### 3.4 Native GUI Framework (`src/gui/`) — *Complex*
* The Win32 GUI layer is deeply coupled to Windows window handles (`HWND`), Windows message loops (`GetMessage`, `DispatchMessage`), GDI device contexts (`HDC`), and Windows Common Controls (`comctl32`).
* Making the GUI cross-platform requires either:
  1. A multi-platform webview layer (e.g. native Webview bindings).
  2. A lightweight cross-platform rendering backend (e.g. SDL3 / Skia / Dear ImGui).
  3. Platform-specific backends (Win32 for Windows, GTK for Linux, Cocoa for macOS).

---

## 4. Writing Cross-Platform EZ Code

To ensure your EZ programs run seamlessly across Windows, Linux, and macOS:

### Dynamic Shared Library Resolution
When using FFI to load external libraries (such as SQLite or custom C libraries), provide cross-platform library search fallbacks:

```ez
# Cross-platform dynamic library loader pattern
libNames = ["sqlite3", "libsqlite3.so.0", "libsqlite3.so", "libsqlite3.dylib", "sqlite3.dll"]
handle = nil

for name in libNames {
    try {
        handle = os_load_lib(name)
        if handle != nil {
            break
        }
    } catch e {
        # continue trying next candidate
    }
}
```

### Standard File Paths
Use forward slashes (`/`) in file paths across all platforms. The EZ runtime and C++ filesystem layer normalize forward slashes across Windows, Linux, and macOS.
