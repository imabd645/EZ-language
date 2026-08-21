// sqlite — SQLite for EZ

> **Version:** 1.0.0  
> **Import:** `use "sqlite"`  
> **File:** `C:\ezlib\sqlite\main.ez`  
> **Requires:** nothing — `winsqlite3.dll` ships with Windows

---

## Overview

SQLite for EZ over **pure FFI**. There is no C++ in this package and no DLL to build:
it calls the SQLite C API directly, and falls back to the copy of SQLite that Windows
already ships in `System32`. Install is `use "sqlite"`.

```ez
use "sqlite"

db = connect("app.db")
db.exec("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)")
db.exec("INSERT INTO users (name, age) VALUES (?, ?)", ["ana", 30])

rows = db.query("SELECT * FROM users WHERE age > ?", [18])
get r in rows { out r["name"] }

db.close()
```

- **Prepared statements** with positional (`?`) and named (`:name`) parameters
- **Transactions** that roll back on error, and nest via savepoints
- **Migrations** tracked in the database itself
- **Real errors**: SQLite's own message, the code name, and the extended code that
  tells `UNIQUE` apart from `FOREIGN KEY`
- **Full type mapping**, including blobs and the whole int64 range on read

---

## Structure

One concern per file. The layering is real: each file only uses the ones below it.

| File | Responsibility |
|---|---|
| `main.ez` | Public surface: `connect`, `with_db`, `version`, `backend`. Nothing else. |
| `src/ffi.ez` | The binding layer. The **only** file that touches `os_load_lib`/`os_call`/`os_alloc`. Absorbs every FFI quirk. |
| `src/errors.ez` | Result codes → diagnosable errors. Code names, extended codes, `is_retryable`. |
| `src/types.ez` | The value boundary: every EZ value in, every column out. Where injection is prevented. |
| `src/statement.ez` | Prepared statements: bind/step/rows, and handle lifetime. |
| `src/connection.ez` | The `Database` model: exec/query/transactions/introspection. |
| `src/migrate.ez` | Schema migrations over `PRAGMA user_version`. |

If SQLite's C API or EZ's FFI changes shape, only `src/ffi.ez` should need to move.

---

## Connecting

### `connect(path, options = nil)` → `Database`

`path` is a filename, `":memory:"`, or a `file:` URI (with `uri: true`).

```ez
db = connect("app.db")
db = connect(":memory:")
db = connect("app.db", {"wal": true, "busy_timeout": 10000})
```

| Option | Default | Meaning |
|---|---|---|
| `readonly` | `false` | Open without write access; the file must already exist |
| `create` | `true` | Create the file if missing |
| `uri` | `false` | Treat `path` as a `file:` URI |
| `busy_timeout` | `5000` | ms to wait on a locked database before failing |
| `wal` | `false` | Write-ahead logging: readers stop blocking writers |
| `foreign_keys` | `true` | Enforce `FOREIGN KEY` constraints |

Two defaults differ from raw SQLite, deliberately:

- **`foreign_keys` is ON.** SQLite defaults it *off* for backwards compatibility with
  databases written before the feature existed. That default silently lets orphaned rows
  accumulate — a declared `REFERENCES` that does nothing is worse than none.
- **`busy_timeout` is 5s.** SQLite's default is 0, so the instant a second connection
  touches the file you get `SQLITE_BUSY` instead of a short wait. This matters as soon as
  you run the web framework with `workers`.

Always open with `connect()`, never `Database()` directly — see
[Interpreter bugs](//interpreter-bugs-worked-around-here).

### `with_db(path, fn, options = nil)`

Open, run, close — even if `fn` throws.

```ez
names = with_db("app.db", |db| { give db.query("SELECT name FROM users") })
```

### `db.close()`

Idempotent. Uses `sqlite3_close_v2`, so it is safe to call while statements are still
alive — they are released as they finalize.

---

## Querying

| Method | Returns |
|---|---|
| `db.exec(sql, params)` | rows changed |
| `db.query(sql, params)` | array of dictionaries |
| `db.query_arrays(sql, params)` | array of arrays |
| `db.one(sql, params)` | first row as a dictionary, or `nil` |
| `db.scalar(sql, params)` | first column of the first row, or `nil` |
| `db.exec_many(sql, param_sets)` | total rows changed |
| `db.exec_script(sql)` | number of statements run |

```ez
db.exec("UPDATE users SET age = ? WHERE id = ?", [31, 1])
n     = db.scalar("SELECT count(*) FROM users")
user  = db.one("SELECT * FROM users WHERE id = ?", [1])
```

`params` may be an array (positional), a dictionary (named), or a single value.

```ez
db.query("SELECT * FROM users WHERE age > ?", [18])
db.query("SELECT * FROM users WHERE age > :min", {"min": 18})
db.query("SELECT * FROM users WHERE id = ?", 1)
```

A wrong parameter count is an **error**, not a shrug. SQLite leaves an unbound parameter
as `NULL`, so a query with a missing value silently returns nothing and looks like "no
rows" — which is a genuinely hard afternoon.

### `exec_many` — bulk insert

Compiles the SQL **once** and runs it per parameter set. Re-preparing per row is usually
the dominant cost of a bulk load. Wrap it in a transaction to avoid one fsync per row too:

```ez
db.transaction(|d| {
    d.exec_many("INSERT INTO users (name, age) VALUES (?, ?)",
                [["ana", 30], ["bob", 25], ["cat", 41]])
    give nil
})
```

### `exec_script` — several statements

`sqlite3_prepare_v2` compiles **one** statement and ignores the rest, so passing
multi-statement SQL to `exec()` silently runs only the first — the classic way a
migration half-applies. `exec_script` walks the whole script:

```ez
db.exec_script("
    CREATE TABLE a (x);
    CREATE TABLE b (y);
    INSERT INTO a VALUES (1);
")
```

The split ignores semicolons inside string literals and comments. It is not a full
parser: it does not understand `BEGIN ... END` trigger bodies. Use `exec()` per statement
for those.

---

## SQL Injection

Values are **bound**, never formatted into SQL. There is deliberately no "quote this for
me" helper, because offering one invites building SQL by concatenation.

```ez
db.query("SELECT * FROM users WHERE name = ?", [name])    // safe, always

// Never. No amount of escaping makes this safe.
db.query("SELECT * FROM users WHERE name = '" + name + "'")
```

Bound values are data even when they are valid SQL: `ana'; DROP TABLE users; --` matches
no row, drops nothing, and stores verbatim. There is a test for exactly that.

Identifiers (table and column names) **cannot** be bound — that is SQLite, not this
library. If a table name must be dynamic, check it against a list you control;
`db.columns()` does this by validating against `sqlite_master` first.

---

## Types

| EZ | SQLite | Notes |
|---|---|---|
| `nil` | NULL | |
| `bool` | INTEGER | SQLite has no boolean; reads back as `1`/`0`, not `true`/`false` |
| `integer` | INTEGER | full signed 64-bit range; see [Integers](//integers) |
| `float` | REAL | |
| `string` | TEXT | UTF-8, quotes and all |
| `buffer` | BLOB | embedded NULs survive |

Anything else (a dictionary, an array) is **refused**, not stringified — storing
`"<dictionary>"` and calling it success is not a favour. Serialise it yourself with
`to_json`.

Reads dispatch on the value's **actual** type, not the column's declared type, because
SQLite is dynamically typed: a `TEXT` column can hold an integer, and `SELECT 1` has no
declared type at all.

### Integers

The full signed 64-bit range binds and reads back exactly. Booleans store as `1`/`0` and
read back as integers (SQLite has no boolean type).

Earlier versions of this library refused integers beyond ±2^53, because EZ's FFI marshalled
every outgoing argument through a double and rounded them in transit (`9007199254740993`
became `...992`, int64 max became int64 **min**). The interpreter now marshals integer
arguments through `asInteger()`, so that limit is gone. Unsigned 64-bit values above 2^63
are the only remaining edge: they exceed EZ's signed 64-bit integer and come back negative.

---

## Transactions

### `db.transaction(fn)`

Commits if `fn` returns, rolls back if it throws, and re-raises. This is the only
transaction API that cannot leave one dangling.

```ez
db.transaction(|d| {
    d.exec("UPDATE accounts SET balance = balance - ? WHERE id = ?", [100, 1])
    d.exec("UPDATE accounts SET balance = balance + ? WHERE id = ?", [100, 2])
    give nil
})
```

Nested calls use **savepoints**, so an inner failure rolls back only its own work:

```ez
db.transaction(|d| {
    d.exec("INSERT INTO log VALUES ('start')")
    try {
        d.transaction(|d2| { d2.exec("...")  throw "nope" })
    } catch (e) { }        // inner rolled back; outer still commits
    give nil
})
```

`BEGIN IMMEDIATE` is used, not `BEGIN DEFERRED`. A deferred transaction takes its write
lock at the first write, so two writers can both start, both read, and only discover the
conflict at COMMIT — `SQLITE_BUSY` *after* the work is done. IMMEDIATE takes the lock up
front and waits out `busy_timeout` instead.

### `db.begin(mode)` / `db.commit()` / `db.rollback()` / `db.in_transaction()`

The manual escape hatch. It puts matching every `BEGIN` on every path — including thrown
errors — on you. Prefer `transaction()`.

---

## Statements

For large result sets, step instead of materialising:

```ez
st = db.prepare("SELECT * FROM big_table")
while st.next() {
    row = st.row_dict()
    // ...
}
st.finalize()        // required: until this runs it can hold a read lock
```

| Method | |
|---|---|
| `st.bind(params)` | bind, chainable |
| `st.next()` | advance; `true` if a row is available. (Named `next`, not `step` — `step` is a reserved word in EZ.) |
| `st.row_dict()` / `st.row_array()` | current row |
| `st.all()` / `st.all_arrays()` | every remaining row |
| `st.one()` / `st.scalar()` | first row / first value |
| `st.column_count()` / `st.column_names()` | metadata |
| `st.reset(clear_bindings = false)` | re-run |
| `st.finalize()` | release; idempotent |

Duplicate column names (`SELECT a.id, b.id`) would collapse into one dictionary key, so
the second is suffixed (`id:1`) rather than dropped. Alias your columns if you care.

---

## Migrations

Tracked in `PRAGMA user_version` — an integer in the database header — so a fresh
database is already at version 0 with no table to set up.

```ez
migrations = [
    {"version": 1, "name": "users", "up": |db| {
        db.exec("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)")
    }},
    {"version": 2, "name": "add email", "up": |db| {
        db.exec("ALTER TABLE users ADD COLUMN email TEXT")
    }}
]

applied = migrate(db, migrations)      // -> [1, 2] first run, [] after
```

- Each migration runs in a transaction **with its own version bump**. If they were
  separate, a crash between them would leave the schema changed but the version stale,
  and the next run would apply it twice.
- The whole set is validated before any of it is applied, so a typo in migration 5 does
  not strand you halfway.
- Versions must be unique integers > 0; input order does not matter.
- `migrate()` is idempotent — call it on every startup.

`schema_version(db)`, `set_schema_version(db, v)`, `pending(db, migrations)`.

---

## Errors

Every failure throws a formatted message carrying the code name, SQLite's own text, and
the SQL:

```
sqlite: SQLITE_CONSTRAINT/SQLITE_CONSTRAINT_UNIQUE: UNIQUE constraint failed: users.name  [sql: INSERT INTO users (name) VALUES (?)]
```

The extended code is the useful part: `SQLITE_CONSTRAINT` is a bare `19` whether you
violated a UNIQUE index or a foreign key.

Helpers from `src/errors.ez`: `code_name(rc)`, `extended_code_name(rc)`, `is_ok(rc)`,
`is_retryable(rc)` (true only for `BUSY`/`LOCKED` — the codes worth retrying), and
`is_constraint_violation(rc)`.

---

## Introspection

`db.last_insert_id()`, `db.changes()`, `db.total_changes()`, `db.tables()`,
`db.table_exists(name)`, `db.columns(table)`, `db.integrity_check()`.

---

## Using it with the web framework

Handlers are serialized within a process, so one connection per process is fine. With
`workers > 1` each worker is a separate process, so **give each its own connection** and
turn on WAL:

```ez
use "web"
use "sqlite"

db = connect("app.db", {"wal": true})   // one per worker process

app = WebApp()
app.get("/users", |req| { give db.query("SELECT * FROM users") })
app.run(port=8080, workers=4)
```

Without WAL, writers block readers and the workers spend their time waiting on each other.

---

## Interpreter bugs worked around here

Both are reproducible without this library, and both are worth knowing about:

**Throwing from `init()` crashes the VM.** An exception escaping a model's constructor
causes an access violation at shutdown — after the error is caught and handled:

```ez
model M { init() { throw "x" } }
try { M() } catch (e) { }        // prints fine, then 0xc0000005
```

So `Database.init` catches everything and stashes it in `_init_error`, and `connect()`
raises it from outside the constructor. **Use `connect()`, not `Database()`**, or an open
failure will be silent.

**`finally` does not run when a `give` leaves the block.**

```ez
try { give x } finally { cleanup() }        // cleanup() never runs
try { ... } catch (e) { give y } finally { cleanup() }   // also skipped
```

Only a `try`/`catch` that falls off the end runs its `finally`. For a database library
that is the difference between finalizing a statement and leaking it, so nothing here
relies on `finally`: the pattern throughout is capture-result, capture-error, clean up,
then re-throw. See `with_statement` in `src/statement.ez`.

---

## Notes & Limits

- **Windows only**, like the rest of the stack.
- **`winsqlite3.dll` is Microsoft's copy** and its version is whatever your Windows ships
  (3.51.1 here). Drop a 64-bit `sqlite3.dll` on `PATH` to pin your own — it is preferred
  automatically. `backend()` reports which one is loaded. A 32-bit DLL will not load into
  a 64-bit process.
- **No connection pool.** The VM is single-threaded per process; a pool would buy nothing.
- **No user-defined SQL functions or hooks.** Those need C callbacks, which is possible
  via `os_ffi_create_callback` but not implemented.

---

## Tests

```
cd C:\ezlib\sqlite
ez test_sqlite.ez
```

124 assertions: type round-trips (including int64 reads, blobs, unicode, embedded
quotes, and the full int64 range binding exactly), parameter-count and named-parameter validation, SQL
injection, error codes and extended codes, statement lifetime, `exec_many`,
`exec_script` (including semicolons inside literals and comments), transaction commit and
rollback, nested savepoints, migrations (ordering, idempotence, rollback on failure), and
open failures.
