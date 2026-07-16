# EZ Web Framework (`lib/web.ez`) - Documentation

The EZ Web Framework is a production-ready, Flask-inspired web server and template engine built entirely in the EZ Programming Language. It communicates natively with the Windows Socket API (Winsock2) via FFI bindings and features a custom template parsing compiler.

---

## Table of Contents
1. [Core Features](#core-features)
2. [Quick Start](#quick-start)
3. [Global Helpers](#global-helpers)
4. [The `WebApp` Application Class](#the-webapp-class)
5. [Request Object Structure](#request-object-structure)
6. [Middlewares & Sessions](#middlewares--sessions)
7. [The Template Engine Syntax](#the-template-engine-syntax)
8. [Winsock2 FFI & Native Sockets](#winsock2-ffi--native-sockets)

---

## Core Features
* **Zero External Dependencies**: Implemented in pure EZ using raw Win32 Winsock FFI.
* **Flask-Style API**: Register routes easily using `app.get()`, `app.post()`, and middleware chains.
* **Auto Session Management**: Robust session tracking using encrypted/randomized browser cookies.
* **Full-Featured Template Engine**: Supports inheritance (`extends`/`block`), loops (`for`), conditionals (`if`), and components (`include`).
* **Binary Workarounds**: Includes `indexOfSafe` to resolve the native binary `indexOf` start position limitation.

---

## Quick Start

```ez
use "lib/web.ez"

app = WebApp()

# Route parameter route
app.get("/hello/<name>", |req| {
    name = req["params"]["name"]
    give "<h1>Hello, " + name + "!</h1>"
})

app.run(8080)
```

---

## Global Helpers

### `render_template(path, ctx)`
Reads a file and compiles it using the custom template compiler with the provided context dictionary.
* **Parameters**:
  * `path` (string): Absolute or relative file path to the HTML template.
  * `ctx` (dictionary): Key-value pairs of variables passed to the template.
* **Returns**: Compiled HTML string.
* **Usage**:
  ```ez
  give render_template("templates/index.html", { "user": "Bob" })
  ```

### `redirect(url)`
Generates an HTTP 302 Redirect payload structure recognized by the server dispatcher.
* **Parameters**:
  * `url` (string): The path or domain to redirect the user to.
* **Usage**:
  ```ez
  give redirect("/dashboard")
  ```

### `indexOfSafe(s, search, start)`
A critical bug-fix helper that resolves the native C++ `indexOf` arity limitation by copying standard slices to find occurrences starting from a given index position.
* **Usage**:
  ```ez
  vStart = indexOfSafe(tmpl, "{{", i)
  ```

---

## The `WebApp` Class

### Methods
* **`init()`**: Sets up application routing maps, session storage, and registers default HTTP error handlers.
* **`use(handler)`**: Registers a middleware function.
* **`get(path, handler)`**: Registers a `GET` route.
* **`post(path, handler)`**: Registers a `POST` route.
* **`run(port)`**: Initializes WinSock2, binds the socket to `0.0.0.0`, and enters the multi-threaded listening server loop.

---

## Request Object Structure
Every route handler and middleware receives a `req` request dictionary containing the following keys:

| Key | Type | Description |
| :--- | :--- | :--- |
| `method` | String | The HTTP method (e.g. `"GET"`, `"POST"`). |
| `path` | String | The requested URL path without query arguments (e.g. `"/signup"`). |
| `fullPath` | String | The raw request URL path including query arguments. |
| `body` | String | The raw parsed body string. |
| `headers` | Dictionary | Lowercased HTTP Request headers. |
| `query` | Dictionary | Key-value mapping of URL parameters (e.g. `?name=alice`). |
| `form` | Dictionary | Key-value mapping of parsed POST body form-data. |
| `params` | Dictionary | Key-value mapping of route pattern matches (e.g. `/user/<id>`). |
| `cookies` | Dictionary | Raw browser cookie keys and values. |
| `session` | Dictionary | Persistent session dictionary mapped to the browser. |

---

## Middlewares & Sessions
Middlewares execute sequentially on every incoming request. If a middleware returns a dictionary or string, it halts execution and sends that response directly to the browser.

```ez
# Middleware: Restrict access to secure routes
app.use(|req| {
    path = req["path"]
    when path != "/login" and not has_key(req["session"], "username") {
        give redirect("/login") # HALTS execution and redirects
    }
    give nil # CONTINUES to the route handler
})
```

### Session Handling
Sessions are stored in memory (`app.sessions`) and identified on the client side via the `ez_session` cookie. Cookie IDs are uniquely constructed using runtime clock counters and timestamps to avoid collision.

---

## The Template Engine Syntax

### 1. Variables
Renders raw or stringified variables from the context:
```html
<p>Welcome, {{ user.name }}</p>
```

### 2. Conditionals (`if / else / endif`)
Renders elements depending on truthiness of context expressions:
```html
{% if error %}
    <div class="alert">{{ error }}</div>
{% else %}
    <p>No issues logged.</p>
{% endif %}
```

### 3. Iterators (`for / endfor`)
Loops through arrays or key-value dictionary elements:
```html
{% for u in users %}
    <li>ID: {{ u.id }} - Name: {{ u.name }}</li>
{% endfor %}
```

### 4. Template Inheritance (`extends / block / endblock`)
Defines base structures to avoid HTML duplication.
* **`base.html`**:
  ```html
  <html>
    <body>
       {% block content %}{% endblock %}
    </body>
  </html>
  ```
* **`child.html`**:
  ```html
  {% extends "base.html" %}
  {% block content %}
     <h1>Child Page Overrides Base!</h1>
  {% endblock %}
  ```

---

## Winsock2 FFI & Native Sockets

The underlying network logic loads the Windows Socket Library (`ws2_32.dll`) and maps C-functions into EZ tasks:

```ez
model WebFFI {
    static wsaStartup  = os_get_func(__ws32, "WSAStartup")
    static socket      = os_get_func(__ws32, "socket")
    static bind        = os_get_func(__ws32, "bind")
    static listen      = os_get_func(__ws32, "listen")
    static accept      = os_get_func(__ws32, "accept")
    static recv        = os_get_func(__ws32, "recv")
    static send        = os_get_func(__ws32, "send")
    static closesocket = os_get_func(__ws32, "closesocket")
}
```

Every incoming client socket spawns as a background fiber/thread in the runtime via:
```ez
spawn(WebApp._handleClient, clientSocket, self)
```
This isolates each request context, keeping the server responsive and multi-threaded natively.
