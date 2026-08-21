# testing

A unit testing framework for EZ. Assertions, suites, hooks, spies and stubs,
with coloured reporting.

```ez
use "testing"

describe("arithmetic", || {
    it("adds", || { assertEqual(1 + 1, 2) })
    it("throws on bad input", || { assertThrows(|| parse("nonsense")) })
})

runTests()
```

```
═══ EZ Test Framework ═══

  arithmetic
  ✓ adds
  ✓ throws on bad input

═══════════════════════════════
  2 passed

  Duration: 1ms
═══════════════════════════════
```

## Install

```
ez install testing
```

No dependencies.

## Assertions

Each takes an optional trailing `message` that replaces the generated one.

| | |
|---|---|
| `assert(value)` | value is truthy |
| `assertFalse(value)` | value is falsy |
| `assertEqual(actual, expected)` | `==` |
| `assertNotEqual(actual, notExpected)` | `!=` |
| `assertDeepEqual(actual, expected)` | structural, for lists and dictionaries |
| `assertNull(value)` / `assertNotNull(value)` | |
| `assertType(value, "string")` | matches `type()` |
| `assertContains(haystack, needle)` | substring |
| `assertInArray(array, value)` | membership |
| `assertThrows(fn)` | and optionally `assertThrows(fn, "expected text")` |
| `assertDoesNotThrow(fn)` | |
| `assertApproxEqual(actual, expected)` | floating point; tolerance defaults to `0.0001` |
| `assertGreaterThan(value, threshold)` | |
| `assertLessThan(value, threshold)` | |

An assertion throws on failure. The runner catches it, marks the test failed and
carries on with the rest — one broken test does not stop the suite.

## Suites and tests

```ez
describe("a group", || {
    it("does a thing", || { … })
    testCase("same as it", || { … })
    skipe("not yet", || { … })          // reported as skipped, not run
})
```

`suite` is an alias for `describe`. Suites nest, and the report indents them.

### Hooks

```ez
describe("with setup", || {
    beforeAll(|| { openDatabase() })
    beforeEach(|| { clearTables() })
    afterEach(|| { rollback() })
    afterAll(|| { closeDatabase() })

    it("…", || { … })
})
```

A hook that throws fails the test it belongs to rather than taking down the run.

## Spies and stubs

```ez
s = spy()                      // records calls, returns nil
s = spy(|args| { … })          // records calls AND calls through
s = stub("fixed")              // records calls, always returns "fixed"
s = mock()                     // spy over a no-op
```

A bare `spy()` has nothing to call through *to*, so it records and returns its
configured value rather than trying to invoke nothing.

The callable returned by `spy` takes a **list** of arguments:

```ez
s = spy()
s([1, 2])
```

To inspect it, keep the `Spy` itself:

```ez
recorder = Spy(nil)
recorder.call([1])
recorder.callCount()           // 1
recorder.getCall(0)            // { args, timestamp }
recorder.calledWith([1])       // true
recorder.returns("x")          // set the return value
recorder.reset()               // forget the calls
```

## Running

```ez
runTests()                     // runs everything registered, prints the report
runTests({ "silent": true })   // no output, just the counts
run()                          // runs, then exits with 1 if anything failed
clear()                        // forget all registered suites
```

`run()` is what you want in CI — a non-zero exit status is how the pipeline
finds out.

## Two ways to import

```ez
use "testing"

describe(…)        assertEqual(…)        runTests()
```

Everything is also reachable through a namespace, if the bare names would
collide with your own:

```ez
test["describe"](…)
test["assertEqual"](…)
```

and under `t`-prefixed aliases — `tDescribe`, `tIt`, `tAssertEqual`, `tRunTests`,
`tSpy`, and so on for every name above.

## Changes in 1.0.1

1.0.0 had three defects that made it hard to use:

- **Nothing was exported.** `use "testing"` brought in no names at all — the
  framework was only reachable via `use "testing/main.ez"`. Every public
  function, the `Spy` model, the `test` namespace and the `t*` aliases are now
  exported.
- **No parameter had a default**, so every call had to pass every argument:
  `assertEqual(a, b, nil)`, `runTests(nil)`, `spy(nil)`. The optional trailing
  arguments are now genuinely optional. The bodies already handled `nil`, so
  nothing that worked before behaves differently.
- **`spy()` with no target crashed** on the first call, trying to invoke `nil`.

Also fixed: the runner assigned to a local named `suite`, which in EZ replaces
a module-level task of the same name for the whole process — so `suite()` broke
permanently once any test had run.

## License

MIT
