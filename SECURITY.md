# Security Policy

EZ is a young, actively-developed language with a C++ interpreter/VM core, an FFI layer, and a growing package ecosystem (`ezlib`) distributed through packages.ez-lang.site. Because EZ embeds native code execution (FFI, Win32 bindings) and a garbage-collected runtime, security issues here can have real impact on anyone running EZ scripts or installing packages — please report responsibly.

## Supported Versions

EZ does not yet have a formal LTS/multi-branch support policy. Security fixes are guaranteed only for the **latest GitHub Release** and the **latest commit on `main`**. If you're running an older release, please update before reporting — the issue may already be fixed.

| Version                        | Supported |
| ------------------------------- | --------- |
| 4.0 (latest release)            | ✅        |
| `main` branch (unreleased)      | ✅        |
| < 4.0                           | ❌ (best-effort only) |
| `ezlib` packages on the registry | ✅ (see scope below) |

## Reporting a Vulnerability

EZ uses **GitHub Issues** for bug reports and feature requests, and welcomes **pull requests** for fixes and improvements — but **security vulnerabilities are the one exception**. Please do not open a public GitHub issue or PR that reveals a vulnerability before it's been triaged.

Instead, report privately using one of these channels:

1. **GitHub Security Advisories (preferred):** open a draft advisory at [github.com/imabd645/EZ-language/security/advisories/new](https://github.com/imabd645/EZ-language/security/advisories/new). This keeps the report private until a fix ships.
2. **Email:** if you don't have a GitHub account or prefer email, contact the maintainer directly (see the profile linked from the repo) with `[SECURITY] EZ` in the subject line.

When reporting, please include:

- A clear description of the vulnerability and its impact
- Steps to reproduce, or a minimal `.ez` script / proof-of-concept that triggers it
- The affected component (VM, GC, FFI, a specific stdlib/ezlib package, the package manager/bundler, etc.)
- Your OS version and compiler details
- Any suggested fix or mitigation, if you have one

### What to expect

- **Acknowledgment:** within a few days of your report.
- **Triage:** an initial assessment of severity and affected scope.
- **Fix & disclosure:** since this is a solo-maintained project, timelines depend on severity and complexity, but the goal is to ship a fix and credit the reporter (unless you prefer to stay anonymous) before any public disclosure. Please allow a reasonable window for a fix before disclosing publicly.

### If you already have a fix

If you've found a vulnerability *and* written a patch, don't open the PR directly against `main` — it would disclose the issue publicly before it's reviewed. Instead:

1. Open a draft GitHub Security Advisory (link above) describing the issue.
2. Attach your patch there, or mention you have one ready — advisories support a private fork/PR workflow so the fix can be reviewed and merged confidentially, then released together with the advisory.

For everything **non-security** — bug reports, feature requests, general fixes — regular GitHub Issues and pull requests against `main` are the right channel and are very welcome.

## Scope

**In scope:**
- The EZ interpreter and bytecode VM (parsing, compilation, execution)
- The garbage collector (memory corruption, use-after-free, double-free, GC-triggered crashes)
- The FFI layer (`ezffi`) — unsafe native calls, type confusion across the FFI boundary, callback handling
- Win32/GUI bindings
- The bundler and package manager (path traversal, arbitrary code execution during install/build, dependency confusion)
- Official `ezlib` packages published on the registry (packages.ez-lang.site), especially networking, database, and security-related packages (e.g. HTTP/web libraries, the WhatsApp Web / Noise-protocol client, serial/hardware access)
- The package registry itself, if you find issues there (auth, upload validation, package tampering)

**Out of scope / lower priority:**
- Vulnerabilities that require an attacker to already have arbitrary code execution on the victim's machine
- Issues in third-party `ezlib` packages not maintained by the core project (report to that package's author instead, if applicable)
- Denial-of-service via pathological but non-malicious input (e.g. deeply nested scripts causing stack exhaustion) — still welcome as a report, just not treated as critical
- Missing security headers or hardening suggestions on the documentation site with no demonstrated exploit

## Known Risk Areas

To help focus reports, areas that are inherently higher-risk given EZ's design:

- **Memory safety in the VM/GC** — EZ is implemented in C++17; bugs in the Bacon-Rajan cycle collector, slot array handling, or object lifetime management can lead to memory corruption.
- **FFI boundary** — `ezffi` calls into native libraries via libffi; incorrect type mapping, callback misuse, or missing bounds checks here can lead to crashes or memory unsafety. The `DoubleFree`/`UseAfterFree` exception hierarchy exists specifically to catch classes of these bugs — reports of gaps in that coverage are valuable.
- **Package supply chain** — since `ezlib` packages are installed and executed as part of EZ programs, malicious or compromised packages on the registry are a realistic threat vector. Reports about install-time code execution, typosquatting, or registry integrity are welcome.
- **Networking/parsing code** — HTTP, data-format, and protocol libraries (e.g. the Noise-protocol WhatsApp client) that parse untrusted input over the network.

## Disclosure Policy

This project follows a coordinated disclosure model: please give the maintainer a reasonable opportunity to fix the issue before any public write-up, blog post, or disclosure. Credit will be given to reporters in release notes / the advisory unless you request otherwise.

Thank you for helping keep EZ and its users safe.
