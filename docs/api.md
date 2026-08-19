# API Reference — Built-in Functions

Everything on this page is a **native C++ function**, registered directly in
`src/builtins/Builtins_*.cpp` (and `src/gui/*.cpp` for
GUI, Windows-only), always available with no `use` statement required. For
higher-level library functions (ORM, web framework, fluent GUI, crypto
hashing, etc.), see [09_Builtin_Libraries.md](09_Builtin_Libraries.md) — those live
in the external `ezlib` package registry, written in EZ itself.

Platform note: everything below is cross-platform **except** the `gui_*`
family, which is Windows-only.

## Output, input & console

| Function | Description |
|---|---|
| `out expr` | Print a value with a trailing newline (statement, not a function call) |
| `print(val)` | Print without a trailing newline |
| `input(prompt)` / `__input__` | Read a line from stdin |
| `color(code)` | Set console text color (Windows console API; ANSI fallback on POSIX) |
| `reset()` | Reset console color |
| `clear()` | Clear the console screen |
| `gotoxy(x, y)` | Move the console cursor |
| `getch()` | Read a single character without echo |
| `argv` | Global array of command-line arguments passed after the script name |
| `scriptName` | Global string — the path of the running script |

## Type conversion & inspection

| Function | Description |
|---|---|
| `str(val)` | Convert any value to its string representation |
| `num(val)` | Parse a string/value to a number; `0` on failure |
| `type(val)` / `typeOf(val)` | Runtime type name as a string |
| `len(val)` | Length of a string, array, or dictionary |
| `os()` / `get_os()` | Current OS name: `"windows"`, `"macos"`, `"linux"`, or `"posix"` |
| `system(command)` | Run a shell command; returns its exit code |

**Runtime type names** (`typeOf`): `"nil"`, `"bool"`, `"integer"`,
`"float"`, `"string"`, `"array"`, `"dictionary"`, `"function"`, `"model"`,
`"instance"`, `"future"`, `"buffer"`, `"mutex"`, `"interface"`, `"super"`.
`typeOf` distinguishes `"integer"` (whole numbers, `long long`) from
`"float"` (doubles); both satisfy numeric operators.

## String functions

| Function | Description |
|---|---|
| `substr(s, start, len)` / `substring(s, start, end)` | Extract a substring |
| `split(s, delim)` | Split a string into an array |
| `join(arr, delim)` | Join an array into a string |
| `bytesToString(buf)` | Decode a byte buffer/array as a string |
| `upper(s)` / `toUpper(s)` | Uppercase |
| `lower(s)` / `toLower(s)` | Lowercase |
| `trim(s)` | Strip leading/trailing whitespace |
| `replace(s, old, new)` | Replace all occurrences of a substring |
| `contains(s, sub)` | Substring check (also works on arrays for membership) |
| `startsWith(s, prefix)` | Prefix check |
| `endsWith(s, suffix)` | Suffix check |
| `indexOf(s_or_arr, val)` | First index, or `-1` |
| `ord(char)` | Character → ASCII code |
| `chr(code)` | ASCII code → character |
| `hex_to_bytes(hexStr)` | Hex string → byte buffer |
| `b64url_encode(data)` / `b64url_decode(data)` | URL-safe Base64 (standard-alphabet base64 is **not** a builtin — use the `ezlib` `crypto` package) |

### Regular expressions

Engine: `std::regex` in ECMAScript mode — no dotall, no named groups. The
`re_*` family reports match **positions**; the legacy `re*` family does not
(so it can't correctly walk repeated matches — searching for a match's own
text finds the first *literal* occurrence, not necessarily the right one).

| Function | Description |
|---|---|
| `re_test(text, pattern[, flags])` | Does the pattern occur anywhere? |
| `re_full_match(text, pattern[, flags])` | Does it match the whole string? |
| `re_find(text, pattern[, flags, start])` | First match at/after `start`, or `nil` |
| `re_find_all(text, pattern[, flags, limit])` | Every non-overlapping match |
| `re_replace(text, pattern, repl[, flags, limit])` | Replace; `limit` 0 = all; `$1`-`$9`/`$&` supported in `repl` |
| `re_split(text, pattern[, flags, limit])` | Split on the pattern |
| `re_escape(text)` | Escape metacharacters for a literal match |
| `re_valid(pattern)` | Does the pattern compile? |
| `reMatch(text, pattern)` | Legacy: whole-string match test |
| `reSearch(text, pattern)` | Legacy: first match as `[whole, group1, ...]` |
| `reReplace(text, pattern, repl)` | Legacy: replace all matches |

`re_find`/`re_find_all` return dictionaries shaped
`{"text", "start", "end", "groups"}`. A capture group that did not
participate is `nil`, not `""`. An invalid pattern throws `RegexError`
rather than silently reporting "no match". Flags combine in one string:
`"i"` case-insensitive, `"m"` multiline.

## Array & dictionary functions

| Function | Description |
|---|---|
| `push(arr, val)` | Append to array |
| `pop(arr)` | Remove and return the last element |
| `insert(arr, idx, val)` | Insert at index |
| `remove(arr, val_or_idx)` | Remove an element |
| `reverse(arr)` | Reverse in place |
| `sort(arr)` | Sort in place |
| `slice(arr, start, end)` | Return a sub-array (also works on strings) |
| `range(start, end[, step])` | Generate an array of numbers |
| `filter(arr, fn)` | Elements where `fn` returns true |
| `map(arr, fn)` | Transform each element |
| `reduce(arr, fn, init)` | Fold to a single value |
| `forEach(arr, fn)` | Call `fn` per element, no return value |
| `every(arr, fn)` | True if `fn` is true for every element |
| `some(arr, fn)` | True if `fn` is true for any element |
| `find(arr, fn)` | First element matching `fn`, or `nil` |
| `keys(dict)` | Array of dictionary keys |
| `values(dict)` | Array of dictionary values |
| `has_key(dict, key)` | Boolean key check |
| `dictRemove(dict, key)` | Delete a key |
| `properties(instance)` | Array of an instance's property names |

Arrays support 0-based indexing with negative indices (`arr[-1]` = last
element) and slicing (`arr[1:3]`); dictionary keys are strings.

## JSON

| Function | Description |
|---|---|
| `parse_json(str)` | Parse a JSON string into an EZ value |
| `to_json(val)` | Serialize an EZ value to a JSON string |

## File I/O

| Function | Description |
|---|---|
| `readFile(path)` | Read an entire file as a string |
| `writeFile(path, content)` | Write/overwrite a file |
| `appendFile(path, content)` | Append to a file |
| `readLines(path)` | Read a file as an array of lines |
| `writeLine(path, line)` | Write a single line |
| `appendLine(path, line)` | Append a single line |

### The `File` class

For streaming, appending over time, seeking, or managing files:

```ez
f = File("app.log", "a")     # modes: "r" "w" "a" "rb" "wb" "ab" "rw"
f.write("a line\n")
f.flush()                    # push to disk without closing
f.close()
```

`File()` **throws** `FileNotFoundError` if the path cannot be opened — it
never returns a closed handle, so wrap it in `try` rather than testing the
result.

| Method | Description |
|---|---|
| `readLine()` | One line without its newline; `nil` at EOF |
| `read(n)` | Up to `n` bytes; `""` at EOF |
| `readAll()` | Everything remaining |
| `write(s)` / `writeLine(s)` | Write, optionally with a trailing newline |
| `flush()` | Push buffered writes to the OS |
| `seek(offset)` / `tell()` | Move / report the file position |
| `size()` | Size in bytes; restores position, safe mid-write |
| `isOpen()` / `eof()` | State checks |
| `close()` | Close the handle |

| Static | Description |
|---|---|
| `File.exists(path)` | Does it exist? |
| `File.size(path)` | Size in bytes |
| `File.rename(from, to)` | Rename, replacing the destination if present |
| `File.remove(path)` | Delete; returns `false` if already absent, throws only on real failure |
| `File.delete(path)` | Alias for `File.remove` |

## HTTP client (via libcurl)

| Function | Description |
|---|---|
| `http_get(url[, headers])` | HTTP GET |
| `http_post(url, body[, headers])` | HTTP POST |
| `fetch(url[, options])` | Generic HTTP request (`method`, `body`, `headers` in an options dict) |
| `url_encode(str)` / `url_decode(str)` | URL component encoding |
| `http_parse_request(raw)` | Parse a raw HTTP request string |

> `http_put`, `http_delete`, and `startServer(port, handler)` are **not**
> present as C++ builtins. Use `os_call`/FFI or an `ezlib` package
> (e.g. `serve`) for those.

## Math functions

| Function | Description |
|---|---|
| `floor(x)` / `ceil(x)` / `round(x)` | Rounding |
| `abs(x)` | Absolute value |
| `sqrt(x)` | Square root |
| `pow(base, exp)` | Exponentiation (the `**` operator does the same) |
| `min(a, b)` / `max(a, b)` | Minimum / maximum of two values |
| `rand()` | Random float in `[0, 1)` |
| `randint(lo, hi)` | Random integer in range |

## Buffers (raw byte storage)

| Function | Description |
|---|---|
| `buffer(size_or_string)` | Allocate a raw byte buffer, or build one from a string |
| `buf_size(buf)` | Buffer length in bytes |
| `buf_fill(buf, byteVal)` | Fill with a byte value |
| `buf_copy(src, dst, ...)` | Bulk-copy bytes between buffers |
| `buf_to_str(buf)` | Decode buffer contents as a UTF-8 string |
| `buf[i]` / `buf[i] = v` | Index a buffer to read/write a single byte |

## Concurrency primitives

| Function | Description |
|---|---|
| `spawn(fn, ...args)` | Run `fn(...args)` on a new OS thread → `Future` |
| `await expr` / `sync expr` | Block until a `Future` resolves |
| `awaitAll(futures)` | Block until every future resolves; results in input order |
| `awaitAny(futures)` | Block until the first future settles |
| `isDone(future)` | Non-blocking completion check |
| `cancel(future)` | Cancel a future; awaiting it then throws |
| `waitAsync(ms)` | Non-blocking delay (yields to the event loop) |
| `wait(ms)` / `stop(ms)` | Blocking sleep for `ms` milliseconds |
| `mutex()` | Create a `Mutex` |
| `lock(mu, fn)` | Acquire, run `fn`, release (RAII — releases even on throw) |
| `Atomic(initial)` | Atomic integer: `.get()`, `.set(v)`, `.add(n)`, `.sub(n)` |
| `Channel()` | Blocking queue (see below) |

If a spawned task throws, the error is recorded on its future — `await`
re-raises it as a catchable exception **instance** (read `e.message`; a
local `throw "text"` is caught as a plain string).

### `Channel`

| Method | Description |
|---|---|
| `send(value)` | Append a value, wake one receiver |
| `receive()` | Block until a value is available; `nil` if closed and drained |
| `receiveTimeout(ms)` | Block up to `ms`; `nil` on timeout |
| `tryReceive()` | Take a value only if queued; never blocks |
| `size()` | Values currently queued |
| `close()` / `isClosed()` | Close, waking every receiver |

Since a queued `nil` is indistinguishable from "empty", send a token value
rather than `nil` to signal over a channel.

## Date & time

`clock()` returns raw epoch milliseconds. For calendar work, use
`DateTime`:

```ez
d = DateTime()                              # now
d.format("%Y-%m-%d %H:%M:%S")
d.timestamp()

birthday = DateTime(1998, 6, 12)            # y, m, d
launch   = DateTime(2026, 8, 7, 9, 30, 0)   # y, m, d, h, min, s
```

| Method | Description |
|---|---|
| `year()` `month()` `day()` | Date parts |
| `hour()` `minute()` `second()` | Time parts |
| `weekday()` | Day of week |
| `timestamp()` | Epoch milliseconds |
| `format(fmt)` | `strftime` pattern, local time |
| `diff(other)` | Milliseconds between two `DateTime`s |
| `addMs(n)` / `addSeconds(n)` / `addDays(n)` | Shifted copy |
| `toString()` | Readable form |

`DateTime()` accepts 0, 3, or 6 arguments; anything else is a `TypeError`,
and an impossible date is a `ValueError`.

`Timer` runs a callback on a background thread:

```ez
t = Timer(1000, true)          # every second, repeating
t.onTick(| | { out "tick" })
t.start()
wait(5000)                     # keep the main thread alive, or nothing fires
t.stop()
```

`start()` returns immediately; a script that only calls `start()` and then
exits prints nothing, since the process exits and takes the timer with it.
Hold the main thread open with `wait(ms)` for as long as the timer should
run. Same for a one-shot `Timer(interval)`.

| Method | Description |
|---|---|
| `onTick(fn)` | Set the callback (chainable) |
| `start()` / `stop()` | Start / stop the background thread |
| `isRunning()` | State check |

## Metaprogramming

| Function | Description |
|---|---|
| `getattr(obj, name)` | Get a property by name string |
| `setattr(obj, name, val)` | Set a property by name string |
| `hasattr(obj, name)` | Check if a property exists |
| `eval(code)` | Compile and run a string of EZ source; returns the value of its last expression |

`eval` shares the caller's globals, so it can read and define them. A
lexer/parser error inside `eval` throws (catchable), though the parser also
prints the diagnostic to stderr first. The static type checker cannot see
names created inside an `eval` string, and `eval` should never receive
untrusted input.

## GC control

| Function | Description |
|---|---|
| `gc_disable()` / `gc_enable()` | Toggle the cycle collector |
| `gc_collect()` | Force a collection cycle |
| `gc_set_thresholds(minor, major)` | Set the object-count thresholds that trigger collection |

## Errors & process control

| Function | Description |
|---|---|
| `Exception(message, code)` | Construct a `{message, code, stackTrace}` dictionary |
| `panic(message)` | Raise a fatal, **uncatchable** runtime error (exit code 70) |
| `exit(code)` | Terminate the process immediately |
| `clock()` | Milliseconds since the Unix epoch (wall-clock) |

## Native FFI (`os_*`)

See [ffi-and-gui.md](ffi-and-gui.md) for the full FFI reference, including
`os_load_lib`, `os_get_func`, `os_call`, `os_call_sig`, `os_call_sig_arr`,
`os_ffi_create_callback`, the `os_read_*`/`os_write_*` memory family, and
struct layout helpers. The FFI mechanism itself is cross-platform; only the
specific system DLLs you choose to load (Kernel32, User32, msvcrt, etc.)
are Windows-specific.

## GUI (`gui_*`, Windows-only)

See [ffi-and-gui.md](ffi-and-gui.md#gui-framework) for the raw GUI builtin
catalog (~70 functions across window management, controls, dialogs, menus,
and drawing). GUI registration is compiled out entirely on non-Windows
builds; calling a `gui_*` function there raises an "undefined
variable/function" error.

---

For a line-by-line, source-cited catalog with individual signatures,
tiers of confidence, and worked examples for every builtin above, see
`BUILTINS.md` in the repository root — it is generated from a systematic
pass over `src/builtins/*.cpp` and is the most granular reference available.
