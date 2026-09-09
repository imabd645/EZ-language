# EZ Elegant GUI Documentation (v3.1)

The EZ GUI library is now fully **Object-Oriented**. It supports natural color names and a fluent API.

## 🚀 Getting Started

```ez
use "lib/gui/main.ez"
```

## 🪟 Elegant Windowing

```ez
win = gui.window("My App", 500, 400)
```

## 🎨 Fluent Styling

Instead of complex RGB, use human-readable color names.

```ez
title = win.label("Hi Abdullah", 10, 10, 200, 40)

# Chaining syntax
title.color("green").font("Arial", 20)
```

### Supported Color Names:
`white`, `black`, `gray`, `dark`, `light`, `red`, `green`, `blue`, `neon`, `yellow`, `purple`, `orange`.

## 🧩 Widgets & Components

### Buttons
```ez
# The callback is passed at creation
b = win.button("Submit", 100, 100, 120, 40, || {
    gui.alert("Success!")
})

b.color("blue").font("Segoe UI", 14)
```

### Dropdowns (Combo Boxes)
```ez
d = win.dropdown(200, 200, 150, 30)

# Add options easily
d.add("Male")
d.add("Female")
d.add("Other")

# Get selected value
val = d.selected()
```

### Inputs & Labels
```ez
lbl = win.label("Name:", 10, 50, 100, 20)
inp = win.input(120, 50, 200, 30)

# Get/Set values
curr = inp.value()
inp.text("New Content")
```

## 🏗️ Layout Managers

VBox handles vertical stacking automatically.

```ez
v = win.vbox(15) # 15px padding
v.label("Settings", 100, 30).color("neon")
v.input(200, 30)
v.button("Save", 100, 40, || { ... })
```

## 🧪 Run the Demo
Try `examples/gui_elegant.ez` for a full demonstration of this new elegant API!

## 🖼️ Canvas & Graphics (New in v3.1)
Draw custom 2D graphics natively using the canvas widget.

``ez
c = win.canvas(400, 400)

c.beginDraw()
c.clear(255, 255, 255) # RGB background
c.drawRect(10, 10, 100, 100, "blue")
c.drawCircle(200, 200, 50, "red")
c.drawText("Hello Canvas", 50, 300, 24, "green")
c.endDraw()
``

## 🖱️ Advanced Mouse Events (New in v3.1)
All widgets now support tracking mouse movements and clicks with exact X/Y coordinates.

``ez
c.onMouseMove(|x, y| {
    gui.alert("Moved", X:{x} Y:{y})
})

c.onMouseLeave(|| {
    # No coordinates for leave event
})

c.onMouseDown(|x, y| { })
c.onMouseUp(|x, y| { })
``

## 📁 System Dialogs & Clipboard (New in v3.1)
Access native OS dialogs directly from the gui global namespace.

``ez
# File Dialogs
path = gui.openFileDialog("Select File", "Images\0*.png\0All\0*.*\0")
savePath = gui.saveFileDialog("Save As", "Text\0*.txt\0")

# Color Picker
hexColor = gui.chooseColor() # Returns "#RRGGBB"

# Clipboard
gui.setClipboard("Copied text!")
clipboardText = gui.getClipboard()
``

