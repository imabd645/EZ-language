# `bakchodi` — Automation, Auto-Typing, SMTP Email & Fun Utilities for EZ

A multi-purpose automation and prank package for EZ. Send SMTP emails, auto-type messages (PyAutoGUI style), simulate mouse actions, control PC sound, and automate desktop tasks.

---

## Installation & Usage

```ez
use "bakchodi"
```

---

## 1. SMTP Email Dispatch

Send individual or batch emails via Gmail, Outlook, or custom SMTP servers:

```ez
use "bakchodi"

# Initialize SMTP client
email = Email("smtp.gmail.com", 587, "me@gmail.com", "my-app-password")

# Send email to a list of recipients (or a single string)
email.send("Hello there!", "Test Subject", ["alice@example.com", "bob@example.com"])

# Send rich HTML email
email.sendHtml("<h1>Hello!</h1><p>Welcome to EZ.</p>", "Welcome", "alice@example.com")

# Send multiple emails in a loop with delay
email.spam("Hey bro!", "Important Notice", ["friend@example.com"], 5, 500)
```

---

## 2. Keyboard Message Typer (PyAutoGUI Style)

Automatically types text and presses Enter with configurable delays and repetition:

```ez
use "bakchodi"

# Optional: Countdown so you have time to switch to WhatsApp/Discord/Notepad
countdown(3, "Switch to chat window...")

# Type "hello", press Enter, wait 100ms (default), repeat 5 times
typeMessage("hello", 5)

# Type "hi", press Enter, wait 250ms between attempts, repeat 3 times
typeMessage("hi", 3, 250)

# Realistic human-like typing with character-by-character delay
ghostType("Typing like a real human...", 40)

# Press specific keys
pressKey("enter")
pressKey("escape")

# Hotkeys (e.g., Ctrl+V, Alt+Tab)
hotkey("ctrl", "v")
```

---

## 3. Mouse Automation & Desktop Actions

```ez
use "bakchodi"

# Get screen resolution
res = screenSize()
out "Resolution: " + str(res["width"]) + "x" + str(res["height"])

# Get mouse cursor position
pos = getMousePos()
out "Cursor at: " + str(pos["x"]) + ", " + str(pos["y"])

# Move and click mouse
moveMouse(500, 300)
clickMouse("left", 2)  # Double click

# Prank: Crazy mouse movement for 3 seconds
crazyMouse(3)
```

---

## 4. Audio & Web Actions

```ez
use "bakchodi"

# Beep PC speaker (frequency in Hz, duration in ms)
beep(1000, 200)

# Repeated beeps
beeper(3, 1200, 150)

# Open URL in browser
openUrl("https://github.com")

# The ultimate classic prank
rickroll()
```

---

## API Summary

| Function / Model | Parameters | Description |
| :--- | :--- | :--- |
| `Email(host, port, user, pass)` | `server, port, user, pass` | Creates an SMTP email client. |
| `email.send(body, subject, recips)` | `body, subject, recipients` | Sends email to single string or list of addresses. |
| `email.sendHtml(html, subject, recips)` | `html, subject, recipients` | Sends HTML email. |
| `email.spam(body, subject, recips, n, ms)`| `body, subject, recips, count, delay` | Sends $N$ emails with delay. |
| `countdown(seconds, message)` | `seconds = 3, message` | Displays console countdown. |
| `typeMessage(text, count, delayMs)` | `text, count = 1, delay = 100` | Types text + Enter $N$ times. |
| `ghostType(text, charDelayMs)` | `text, charDelayMs = 40` | Types character-by-character. |
| `pressKey(key)` | `keyStr` | Presses a key (e.g. `"enter"`). |
| `hotkey(k1, k2)` | `k1, k2` | Presses hotkey combination. |
| `moveMouse(x, y)` | `x, y` | Moves cursor to $(x, y)$. |
| `clickMouse(btn, count)` | `btn = "left", count = 1` | Clicks mouse button $N$ times. |
| `getMousePos()` | None | Returns `{ "x": x, "y": y }`. |
| `screenSize()` | None | Returns `{ "width": w, "height": h }`. |
| `beep(freq, durationMs)` | `freq = 1000, duration = 200` | Emits a PC speaker tone. |
| `crazyMouse(seconds)` | `durationSec = 3` | Randomly wiggles cursor. |
| `openUrl(url)` | `url` | Opens URL in default browser. |
| `rickroll()` | None | Plays Rickroll in browser. |
