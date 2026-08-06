# Tests that need the full ezlib

These were under `Test/` but import modules that live in the ezlib standard
library (`C:\ezlib`, or wherever `EZLIB_PATH` points) rather than in this
repository. On a machine without ezlib installed — a fresh clone, or a CI
runner — they fail with "Could not find module", which is a missing dependency
rather than a defect in the interpreter.

They are kept here so they can still be run deliberately, without turning the
core suite red for a reason that has nothing to do with the language.

| File | Needs |
|---|---|
| `test_json.ez` | `json` |
| `test_csv.ez` | `csv` |
| `test_calendar.ez` | `calendar` |
| `test_thread_production.ez` | `thread` |
| `sec13_json.ez` | `json`, `fs` |
| `sec55_json_edge.ez` | `json` |
| `sec35_json_invalid.ez` | `json` |

Run them with ezlib available:

```
ez examples/ezlib_tests/test_json.ez
```

Note `sec35_json_invalid.ez` hangs when the `json` module is missing instead of
reporting the failure, which is one reason it does not belong in an automated
suite.
