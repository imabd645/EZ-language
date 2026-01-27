# API Reference

This document details the standard library functions available in EZ.

## Core Functions

### `out value`
Prints a value to the standard output followed by a newline.
*(Note: keyword-stmt, not function call)*

### `str(value) -> String`
Converts any value to its string representation.

### `num(value) -> Number`
Converts a string or boolean to a number. Returns `0` if conversion fails.

### `type(value) -> String`
Returns the type of the value: `"nil"`, `"boolean"`, `"number"`, `"string"`, `"array"`, `"dictionary"`, `"function"`, `"class"`, `"instance"`, `"future"`.

### `len(collection) -> Number`
Returns the length of a string, array, or dictionary.

### `clock() -> Number`
Returns the processor time consumed by the program in milliseconds.

### `stop(milliseconds)`
Pauses execution of the current thread for the specified duration.

## Math

### `floor(n)`, `ceil(n)`
Rounds a number down or up.

### `abs(n)`
Returns the absolute value.

### `sqrt(n)`, `pow(base, exp)`
Square root and exponentiation.

### `rand()`, `randint(min, max)`
Random number generation. `rand()` returns 0.0-1.0.

## String

### `upper(s)`, `lower(s)`
Converts case.

### `trim(s)`
Removes whitespace from both ends.

### `replace(s, old, new)`
Replaces all occurrences of `old` with `new`.

### `split(s, delimiter) -> Array`
Splits string into an array.

### `slice(s, start, end) -> String`
Returns a substring.

## Collections

### `push(array, value)`
Adds an element to the end of an array.

### `pop(array) -> Value`
Removes and returns the last element of an array.

### `keys(dict) -> Array`
Returns all keys in a dictionary.

### `values(dict) -> Array`
Returns all values in a dictionary.

## Async & Networking

### `spawn(function, args...) -> Future`
Starts a function in a new thread. Returns a Future object.

### `await(future) -> Value`
Waits for a Future to complete and returns its result. Alias: `sync(future)`.

### `fetch(url, [options]) -> Future`
Initiates an HTTP request.
- `url`: String
- `options`: Dictionary (optional)
    - `method`: "GET" or "POST"
    - `body`: Request body string
    - `headers`: Dictionary of headers

### `server(port, handler)`
Starts a simple HTTP server (Windows implementation).
- `handler`: A callback function `|request|` that returns a response string.

## Database (SQLite)

### `db_open(path)`
Opens a connection to a valid SQLite database file.

### `db_query(sql) -> Array`
Executes a SELECT query and returns rows as an array of dictionaries.

### `db_execute(sql)`
Executes a command (INSERT, UPDATE, DELETE).

### `db_close()`
Closes the database connection.
