# Modules (`use`)

```ez
# Import entire file — all its globals become available
use "lib/utils.ez"

# Namespaced import — access via alias.name
use "lib/math.ez" as math
out str(math.sqrt(25))

# Import everything from a module into the current scope
use "lib/helpers.ez" as *

# Import an installed package (after `ez install <name>`)
use "collections"
```

## Path resolution

`use` resolves a path against, in order:

1. `lib/<name>.ez`
2. `lib/<name>/main.ez`
3. the platform `ezlib` install location (e.g. `C:/ezlib/<name>.ez` /
   `C:/ezlib/<name>/main.ez` on Windows), and
4. the literal path given.

This is the same resolution logic `ez bundle` uses to discover transitive
dependencies to pack into a standalone executable — see
[getting-started.md](getting-started.md#bundling-a-standalone-executable).

## Import forms

| Form | Effect |
|---|---|
| `use "path"` | Every global defined in the module becomes directly available |
| `use "path" as alias` | Module contents are namespaced under `alias.name` |
| `use "path" as *` | Every global is imported directly into the current scope (equivalent to the bare form, written explicitly) |
| `use "packageName"` | Resolves an installed `ezlib` package by name |

## Installing packages

```bash
ez install collections
```

then in code:

```ez
use "collections"
```

See [standard-library.md](standard-library.md) for what's available in the
`ezlib` registry, and `ez list` / `ez init <name>` in
[getting-started.md](getting-started.md#cli-reference) for the rest of the
package-management CLI.
