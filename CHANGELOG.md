# Changelog

All notable changes to EZ will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/) once a stable 1.0 API is declared.
Until then, minor versions may include breaking changes.

## [Unreleased]

### Added
- `argv` and `scriptName` globals for command-line argument access
- Enum support with auto-numbering, explicit values, and decorators
- Comprehensive fault-handling tests in the VM

### Changed
- Improved cross-platform support and error handling in `src/`
- Enhanced database library symbol resolution

### Fixed
- SQLite FFI probe step no longer causes CI job failures on some environments

## [4.0.0] - 2026-08-14

### Added
- Full modularization refactor of the interpreter/VM codebase
- libffi-based FFI callbacks
- Flask-style expression decorators (`@app.get("/route")`)
- Go-style Channels and `finally` block support
- `ARCHITECTURE.md`, `AGENTS.md`, `BUILD.md`, `BUILTINS.md`, `SYNTAX.md` documentation

### Changed
- Migrated the event loop to libuv, replacing thread-per-request HTTP handling with
  single-threaded epoll/IOCP multiplexing
- Thread-safety improvements to `EZArray`
- Standard library and documentation moved out of `.gitignore` and into the tracked repo

### Fixed
- Various GUI crashing bugs and GUI function fixes

## [3.x and earlier]

Pre-4.0 history predates this changelog and is not fully itemized here. Highlights:

- Migrated from a tree-walk interpreter to a bytecode VM with computed gotos (~40x speedup)
- Replaced hash-map global variable lookup with a global slot array
- Added a `LOOP_LESS_EQ_LOCAL` superinstruction
- Replaced stop-the-world mark-sweep GC with a Bacon-Rajan cycle-collecting GC
- Refactored `ezffi` with a proper exception hierarchy (`DoubleFree`, `UseAfterFree`, etc.)
- Designed and iterated on an SQLAlchemy-style ORM for EZ, targeting SQLite via FFI
- Moved the `ezlib` package ecosystem from GitHub to a dedicated registry at packages.ez-lang.site

For full commit-level detail on any version, see the [commit history](https://github.com/imabd645/EZ-language/commits/main).

[Unreleased]: https://github.com/imabd645/EZ-language/compare/v4.0.0...main
[4.0.0]: https://github.com/imabd645/EZ-language/releases/tag/v4.0.0
