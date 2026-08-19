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
| **Executable Packager** (`ez package`) | ✅ Fully Supported | ✅ Fully Supported | ✅ Fully Supported |
| **Registry/System Integration** | ✅ Fully Supported | ✅ Fully Supported | ✅ Fully Supported |
| **Console Low-Level APIs** | ✅ Fully Supported | ✅ Fully Supported | ✅ Fully Supported |
| **Desktop Notifications** (`test_notify.ez`) | ✅ Fully Supported | ✅ Fully Supported | ✅ Fully Supported |

---

## 2. The GUI Framework is Windows-Only

The native GUI framework depends on Windows-specific Win32 APIs and will **not** function on Linux or macOS.

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
