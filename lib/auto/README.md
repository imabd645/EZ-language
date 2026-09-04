# EZ Auto

A pure-EZ desktop automation library, providing `pyautogui`-like functionality without requiring external dependencies. It uses FFI (`os_call`) to invoke the native Win32 API.

## Installation
```javascript
use "auto"
```

## Mouse
```javascript
auto.move(500, 500)
auto.click()
auto.click("right")
pos = auto.pos()
print(pos.x, pos.y)
auto.scroll(120)
```

## Keyboard
```javascript
auto.type("Hello World!")
auto.press("enter")
auto.down("shift")
auto.up("shift")
```

## Screen
```javascript
sz = auto.size()
print("Resolution: " + sz.width + "x" + sz.height)
```
