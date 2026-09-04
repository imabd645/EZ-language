# `sysinfo` — System, Hardware & Process Metrics for EZ

A cross-platform system information and hardware monitor for EZ (like Python's `psutil`). Query CPU usage, RAM memory, disk storage, running processes, host information, uptime, and battery health via zero-overhead native Win32 FFI.

---

## Installation & Usage

```ez
use "sysinfo" as sys
```

---

## 1. CPU Metrics

```ez
use "sysinfo" as sys

# Logical CPU core count
cores = sys.cpuCores()
out "CPU Cores: " + str(cores)

# Real-time CPU usage percentage (samples over 100ms)
usage = sys.cpuPercent(100)
out "CPU Usage: " + str(usage) + "%"
```

---

## 2. Memory & RAM Metrics

```ez
use "sysinfo" as sys

# Complete memory dictionary
mem = sys.memory()
out "Total RAM:     " + str(mem["totalMb"]) + " MB"
out "Used RAM:      " + str(mem["usedMb"]) + " MB"
out "Available RAM: " + str(mem["availMb"]) + " MB"
out "Memory Load:   " + str(mem["loadPercent"]) + "%"

# Direct metric helpers
total = sys.ramTotalMb()
used = sys.ramUsedMb()
free = sys.ramFreeMb()
```

---

## 3. Storage & Disk Usage

```ez
use "sysinfo" as sys

# Query specific drive
d = sys.disk("C:")
out "Drive:        " + d["drive"]
out "Total Space:  " + str(d["totalGb"]) + " GB"
out "Used Space:   " + str(d["usedGb"]) + " GB (" + str(d["usedPercent"]) + "%)"
out "Free Space:   " + str(d["freeGb"]) + " GB"
```

---

## 4. Host, Uptime & Battery

```ez
use "sysinfo" as sys

out "Hostname: " + sys.hostname()
out "Username: " + sys.username()
out "Platform: " + sys.platform()

# System uptime
out "Uptime:   " + sys.uptimeHuman()  # e.g., "14d 21h 47m 12s"
out "Hours:    " + str(sys.uptimeHours()) + " hours"

# Battery status (on laptops)
bat = sys.battery()
out "Battery:      " + str(bat["percent"]) + "%"
out "Power Source: " + bat["powerSource"] # "AC Power" or "On Battery"
out "Charging:     " + str(bat["isCharging"])

# Complete JSON summary overview
overview = sys.summary()
out overview
```

---

## 5. Process Enumeration & Control

```ez
use "sysinfo" as sys

# List all running processes
procs = sys.processes()
get p in procs {
    out "PID: " + str(p["pid"]) + " -> " + p["name"] + " (Threads: " + str(p["threads"]) + ")"
}

# Search for a specific process
chromeInstances = sys.findProcess("chrome.exe")
out "Chrome instances running: " + str(len(chromeInstances))

# Check if a program is active
when sys.isProcessRunning("notepad.exe") {
    out "Notepad is open"
}

# Terminate process by PID
# sys.kill(1234)
```

---

## API Summary

| Function | Return Type | Description |
| :--- | :--- | :--- |
| `cpuCores()` | `int` | Number of logical CPU cores. |
| `cpuPercent(sampleMs)` | `float` | CPU utilization percentage ($0.0 - 100.0\%$). |
| `memory()` | `dict` | Memory info (`totalMb`, `usedMb`, `availMb`, `loadPercent`). |
| `ramTotalMb()` | `float` | Total physical RAM in MB. |
| `ramUsedMb()` | `float` | Currently used RAM in MB. |
| `ramFreeMb()` | `float` | Available free RAM in MB. |
| `ramLoadPercent()` | `int` | RAM load percentage. |
| `disk(drive)` | `dict` | Disk info (`totalGb`, `usedGb`, `freeGb`, `usedPercent`). |
| `diskFreeGb(drive)` | `float` | Free disk space in GB. |
| `diskTotalGb(drive)` | `float` | Total disk capacity in GB. |
| `hostname()` | `string` | System computer name. |
| `username()` | `string` | Logged-in operating system user. |
| `uptimeHuman()` | `string` | Human-formatted uptime (e.g. `14d 21h 47m 12s`). |
| `uptimeHours()` | `float` | Uptime in hours. |
| `uptimeMs()` | `int` | Uptime in milliseconds. |
| `battery()` | `dict` | Battery info (`percent`, `powerSource`, `isCharging`). |
| `summary()` | `dict` | Comprehensive snapshot of all system metrics. |
| `processes(maxCount)` | `array` | List of running process objects. |
| `findProcess(query)` | `array` | Finds running processes matching name substring. |
| `isProcessRunning(query)` | `bool` | Returns `true` if matching process is active. |
| `kill(pid)` | `bool` | Terminates process by PID. |
