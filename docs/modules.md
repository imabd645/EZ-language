# Modules (`use`)

```ez
# Import entire file — all its globals become available
use "lib/utils.ez"

# Namespaced import — access via alias.name
use "lib/math.ez" as math
out str(math.sqrt(25))

# Import everything from a module into the current scope
use "lib/helpers.ez" as *

# Import only selected public names, with optional local aliases
use {sqrt, pow as power} from "lib/math.ez"

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
| `use {name, other as local} from "path"` | Import selected exported names, optionally under local aliases |
| `use "packageName"` | Resolves an installed `ezlib` package by name |

Only declarations explicitly marked with `export` are visible to module imports
whose path is a package name or a non-`.ez` module path. File-inclusion imports
ending in `.ez` retain the historical behavior of exposing all top-level names.
An export list can publish an existing declaration without moving it:

```ez
task implementation() { give 42 }
export {implementation as answer}
```

Importing a name that is not exported reports an `ImportError` at the importing
site. Modules are initialized once through the existing module cache, and
circular imports continue to report `CircularImportError`.

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
