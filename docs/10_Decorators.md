# Decorators

A decorator is an `@name` written directly above a `task` or `model` that changes
how it behaves. EZ has five built into the interpreter, plus a `decorator`
keyword for writing your own.

```ez
@cached
task expensive() { ... }

@audited
model Account { ... }
```

Decorators may only precede a `model` or `task` declaration. Anywhere else is a
parse error:

```
Decorators can only be applied to 'model' or 'task' declarations
```

Several may be stacked, one per line.

---

## Built-in decorators at a glance

| Decorator | Applies to | Effect |
|---|---|---|
| `@cached` | task / model method | Remembers the result; recomputed when the instance changes |
| `@audited` | model | Records every property change |
| `@snapshot` | model | Enables `snapshot()`, `rollback()`, `snapshot_diff()` |
| `@persist("file.db")` | model | Saves property writes to SQLite |
| `@ratelimit(n, "period")` | task | Caps how often the task may be called |

Applying one to the wrong kind of declaration is **silently ignored** — the
parser accepts `@audited` on a task, and nothing happens. There is no warning, so
check the target when a decorator seems to do nothing.

---

## `@cached`

Caches the result so repeated calls do not redo the work.

```ez
model Cart {
    init() {
        self.items = 3
        self.price = 10
    }

    @cached
    task total() {
        give self.items * self.price
    }
}

c = Cart()
c.total()     # 30 — computed
c.total()     # 30 — returned from the cache, body did not run
```

The cache lives **on the instance**, so two carts never share a result.

Writing to any property of the instance invalidates its cached results, so the
next call recomputes:

```ez
c.price = 20
c.total()     # 60 — recomputed
```

This invalidation is deliberately conservative: *any* property write clears
*every* cached result on that instance, even one the method never reads. It
recomputes a little more than strictly necessary, which is the right way round
for a cache the caller cannot see.

> **On free functions**, `@cached` does not memoize by argument — calling
> `slowSquare(9)` twice runs the body twice. The cache is keyed to an instance,
> so a task with no `self` has nothing to cache against. To memoize a plain
> function by argument, use `memoize` from the `thread` package.

---

## `@audited`

Records every property change on the model, including the ones made in `init()`.

```ez
@audited
model Account {
    init(owner) {
        self.owner = owner
        self.balance = 0
    }
}

a = Account("ana")
a.balance = 100
a.balance = 250

audit(a)
```

`audit(instance)` returns an array of entries, oldest first. Each is a dictionary:

| Key | Meaning |
|---|---|
| `field` | Property name |
| `old` | Value before the change (`nil` on first assignment) |
| `new` | Value after |
| `via` | Name of the function that made the change |
| `timestamp` | Epoch milliseconds |

```ez
log = audit(a)
log[0]["field"]    # "owner"
log[0]["old"]      # nil
log[0]["new"]      # "ana"
```

| Function | Description |
|---|---|
| `audit(obj)` | Every entry |
| `audit_since(obj, timestamp)` | Entries newer than a timestamp |
| `audit_clear(obj)` | Discard the log |

Calling `audit()` on a model that is not `@audited` is an error, not an empty
list:

```
audit() called on non-@audited model 'Plain'
```

---

## `@snapshot`

Lets you capture an instance's properties and restore them later.

```ez
@snapshot
model Doc {
    init(t) {
        self.title = t
        self.body = ""
    }
}

d = Doc("draft")
saved = snapshot(d)          # a plain dictionary of the current properties

d.title = "final"
d.body = "text"

snapshot_diff(saved, snapshot(d))   # what changed
rollback(d, saved)                  # back to "draft"
```

| Function | Description |
|---|---|
| `snapshot(obj)` | Dictionary copy of the instance's properties |
| `rollback(obj, snap)` | Restore the properties from a snapshot |
| `snapshot_diff(a, b)` | Differences between two snapshots |

A snapshot is an ordinary dictionary, so it can be stored, compared, or written
out as JSON. As with `audit`, calling these on an undecorated model is an error.

---

## `@persist("file.db")`

Writes every property change to a SQLite file, so the values survive a restart.

```ez
@persist("settings.db")
model Settings {
    init() { self.theme = "dark" }
}

s = Settings()
s.theme = "light"        # written to settings.db immediately

# In a later run:
s = Settings.load()      # an instance carrying the saved values
s.theme                  # "light"
```

`Model.load()` is added automatically to a persisted model. Values are stored in
an `EZ_Persist(prop, val)` table, created on first write.

**The path must be a plain string literal.** A variable is not recognised as the
built-in decorator at all — it is treated as a user-defined one, and since no
function named `persist` exists, it fails at the declaration:

```ez
@persist("app.db")     # correct

dbPath = "app.db"
@persist(dbPath)       # Error: Value is not callable: nil
```

Because storage is keyed by property name in one shared table, a `@persist` model
is best used for a **single instance** — application settings, a counter, a
cursor. Two instances of the same persisted model write over each other.

---

## `@ratelimit(count, "period")`

Caps how often a task may run. Calls over the limit throw.

```ez
@ratelimit(3, "minute")
task ping() { give "pong" }

i = 0
while i < 5 {
    try {
        ping()
    } catch (e) {
        out "rate limited"
    }
    i = i + 1
}
# three calls succeed, two are refused
```

Valid periods are `"second"`, `"minute"`, `"hour"`, `"day"`. Anything else falls
back to one minute.

The window slides: the limit is on calls in the *last* period, not on a fixed
clock interval. Both arguments must be literals — a count from a variable is not
recognised as the built-in decorator.

---

## Writing your own

Any `@name` that is not one of the five built-ins is a **user decorator**: the
name is looked up as a function and applied to the declaration.

Declare one with the `decorator` keyword, taking the decorated function and
returning its replacement:

```ez
decorator logged(fn) {
    give | n | {
        out "calling with " + str(n)
        r = fn(n)
        out "returned " + str(r)
        give r
    }
}

@logged
task double(n) { give n * 2 }

double(21)
# calling with 21
# returned 42
```

Stacked decorators apply bottom-up — the one nearest the declaration wraps it
first:

```ez
@outer
@inner
task work() { ... }
# equivalent to outer(inner(work))
```

Order matters when the wrappers do something observable. A guard decorator placed
*below* the one that registers a handler will not protect it, because the
registration happens with the unguarded function.

---

## Gotchas

- **Wrong target is silent.** `@cached` on a model, or `@audited` on a task, is
  accepted and ignored.
- **Built-in decorator arguments must be literals.** `@persist(path)` and
  `@ratelimit(n, unit)` are not recognised as built-ins and become user
  decorators, failing with `Value is not callable: nil`.
- **`@cached` does not memoize free functions by argument** — see above.
- **`@persist` shares one table per file.** One instance per persisted model.
- **Decorators cannot be applied to anything else** — not variables, not `struct`,
  not `enum`.

---

## See also

- [Object-Oriented Programming](05_Object_Oriented_Programming.md) — models, statics
- [Functions and Tasks](04_Functions_and_Tasks.md) — lambdas, which user decorators return
- [Modules and Concurrency](08_Modules_and_Concurrency.md)
