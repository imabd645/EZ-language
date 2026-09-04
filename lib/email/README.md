# email

**SMTP email for EZ — pure libcurl FFI, no C++ glue.**

`email` is an SMTP client library for the [EZ programming language](https://github.com) that lets you send plain-text, HTML, and multipart emails with attachments, directly over `libcurl`. There is no compiled C++ extension in this package — every call into the operating system goes through EZ's built-in FFI (`os_load_lib` / `os_get_func` / `os_call`), all funneled through a single file (`src/ffi.ez`) so the rest of the library stays pure EZ.

```
version: 2.0
```

---

## Table of Contents

- [Features](#features)
- [Requirements](#requirements)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Usage Guide](#usage-guide)
  - [Connecting](#connecting)
  - [Building a Message](#building-a-message)
  - [Sending](#sending)
  - [One-shot Sends](#one-shot-sends)
  - [Attachments](#attachments)
  - [CC, BCC, and Reply-To](#cc-bcc-and-reply-to)
  - [Priority](#priority)
  - [Custom Headers](#custom-headers)
  - [TLS / SSL Configuration](#tls--ssl-configuration)
  - [Debugging](#debugging)
- [Error Handling](#error-handling)
- [Architecture](#architecture)
- [File-by-File Reference](#file-by-file-reference)
- [Resource Safety](#resource-safety)
- [MIME Payload Format](#mime-payload-format)
- [Design Notes & Caveats](#design-notes--caveats)
- [License](#license)

---

## Features

- **Fluent message builder** — chainable `.from()`, `.to()`, `.subject()`, `.html()`, `.text()`, `.attach()`, and more.
- **Plain text, HTML, or both** — automatically builds `multipart/alternative`-style content depending on what you set.
- **Attachments** — base64-encoded `multipart/mixed` MIME parts, wrapped at the RFC-mandated 76 characters per line.
- **CC / BCC** — BCC recipients are delivered via the SMTP envelope only and are never written into message headers.
- **Structured exceptions** — every failure mode is a typed model (`SmtpConnectionError`, `SmtpAuthError`, `AttachmentError`, `EmailError`), never a raw string.
- **TLS by default** — SSL/TLS enabled and certificate verification on out of the box, with Windows native CA store support.
- **Guaranteed cleanup** — temp files, curl handles, and recipient lists are always released, on both success and failure paths.
- **Zero C++** — the entire library is EZ + `libcurl` via FFI.

---

## Requirements

- The **EZ** runtime, with FFI (`os_load_lib`, `os_get_func`, `os_call`) available.
- A 64-bit **libcurl** DLL on `PATH`:
  - `libcurl-4.dll` (tried first — shipped with most EZ distributions), or
  - `libcurl.dll` (fallback name used by some Windows installs).
- `msvcrt.dll` (present on any standard Windows install) — used for temp-file `fopen`/`fclose`/`remove`.

> **Platform note:** the current FFI layer loads Windows DLLs (`libcurl-4.dll`, `libcurl.dll`, `msvcrt.dll`) by name. This release targets Windows; a POSIX build would need `ffi.ez` to load a shared object (`libcurl.so`) instead.

If neither libcurl DLL can be found, the library throws a clear error at import time rather than failing silently later.

---

## Installation

Place the package where your EZ project can reach it and import the public entry point:

```ez
use "email"
```

This pulls in `main.ez`, which internally wires up `src/ffi.ez`, `src/errors.ez`, `src/message.ez`, `src/mime.ez`, and `src/client.ez`. You only ever need to `use "email"` — the `src/` files are implementation details.

---

## Quick Start

```ez
use "email"

client = connect("smtp.gmail.com", 587, "me@gmail.com", "app-password")

client.send(
    Message()
        .from("me@gmail.com")
        .to("alice@example.com")
        .subject("Hello!")
        .html("<h1>Hi Alice!</h1>")
        .attach("report.pdf")
)
```

Or, for a single throwaway email without holding onto a client:

```ez
use "email"

send_quick("smtp.gmail.com", 587, "me@gmail.com", "app-password",
           "me@gmail.com", "alice@example.com", "Hi", "Hello!")
```

---

## Usage Guide

### Connecting

`connect()` returns a configured `SmtpClient` you can reuse for multiple sends:

```ez
client = connect(server, port, username, password)
```

| Argument   | Description                                              |
|------------|------------------------------------------------------------|
| `server`   | SMTP hostname, e.g. `"smtp.gmail.com"`                     |
| `port`     | `465` (implicit SSL/SMTPS), `587` (STARTTLS), or `25` (plain) |
| `username` | Login / email address                                       |
| `password` | Password or App Password                                    |

`SmtpClient` picks `smtps://` vs `smtp://` automatically based on whether `port == 465`.

### Building a Message

`Message` is a fluent builder — every setter returns `self`:

```ez
msg = Message()
    .from("me@gmail.com")
    .to("alice@example.com")
    .cc("bob@example.com")
    .bcc("audit@company.com")
    .subject("Hello!")
    .html("<h1>Hi Alice!</h1>")
    .attach("report.pdf")
    .priority("high")
    .header("X-Campaign", "summer25")
```

Only `from`, `to`, and `subject` are required, plus at least one of `text()` / `html()`. `SmtpClient.send()` calls `msg.validate()` first and throws `EmailError` if anything required is missing — before any network activity happens.

### Sending

```ez
client.send(msg)
```

`send()`:
1. Validates the message.
2. Builds the RFC 2822 MIME payload.
3. Writes it to a uniquely named temp file.
4. Configures and performs the curl SMTP transfer.
5. Cleans up the temp file, curl handle, and recipient list — always.
6. Returns `true` on success, or re-throws a structured exception on failure.

### One-shot Sends

For scripts that only need to send a single plain-text email:

```ez
send_quick(server, port, username, password, fromAddr, toAddr, subject, body)
```

This is sugar for `connect()` + `Message().from(...).to(...).subject(...).text(...)` + `send()`.

### Attachments

```ez
msg.attach("report.pdf")
msg.attach("C:\\Users\\me\\Documents\\invoice.pdf")
```

- Each call attaches one local file.
- File contents are read, base64-encoded, and wrapped at 76 characters per line per the MIME spec.
- The filename embedded in the `Content-Disposition` header is taken from the last path segment (both `\` and `/` separators are handled).
- If a file can't be read, `AttachmentError` is thrown with the file path and reason.
- As soon as one attachment is present, the payload switches to `multipart/mixed`.

### CC, BCC, and Reply-To

```ez
msg.cc("bob@example.com")
msg.cc(["bob@example.com", "carol@example.com"])   // array form also accepted

msg.bcc("audit@company.com")
msg.replyTo("support@example.com")
```

- `to()`, `cc()`, and `bcc()` all accept either a single string or an array of strings.
- **BCC is envelope-only.** BCC addresses are added to the `RCPT TO` list sent to the SMTP server via `CURLOPT_MAIL_RCPT`, but are never written into the message headers — so recipients can't see each other's BCC status by inspecting the raw message.

### Priority

```ez
msg.priority("high")    // X-Priority: 1, Importance: High
msg.priority("normal")  // X-Priority: 3 (no Importance header)
msg.priority("low")     // X-Priority: 5, Importance: Low
```

### Custom Headers

```ez
msg.header("X-Campaign", "summer25")
msg.header("X-Mailer", "email/2.0")
```

Custom headers are appended after the standard headers and before the body.

### TLS / SSL Configuration

`SmtpClient` is secure by default. These fields can be set directly on the client instance before calling `send()`:

| Field            | Default                        | Meaning                                                        |
|------------------|---------------------------------|------------------------------------------------------------------|
| `useSsl`         | `true`                          | Enables `CURLUSESSL_ALL` — require SSL for the entire session   |
| `verifyCert`     | `true`                          | Verifies the server's TLS certificate and hostname               |
| `useNativeCa`    | `true`                          | Uses the Windows native certificate store (`CURLSSLOPT_NATIVE_CA`) |
| `caBundlePath`   | `""`                            | Optional path to a custom `ca-bundle.crt`, used if `verifyCert` is on |
| `minTls`         | `CURL_SSLVERSION_TLSv1_2`       | Minimum negotiated TLS version                                   |
| `verbose`        | `false`                         | Prints the full curl handshake to stdout for debugging           |

```ez
client = connect("smtp.internal.corp", 25, "svc@corp", "pass")
client.verifyCert = false   // ⚠ insecure — internal/self-signed servers only
client.send(msg)
```

> Disabling `verifyCert` disables both peer and host verification. Only do this against trusted internal infrastructure — never over the public internet.

### Debugging

```ez
client.verbose = true
```

Prints the raw curl handshake (`CURLOPT_VERBOSE`) to stdout, useful for diagnosing connection or auth issues.

You can also check which libcurl DLL was actually loaded:

```ez
out backend()   // "libcurl-4.dll" or "libcurl.dll"
```

---

## Error Handling

All exceptions raised by this library are typed models, never raw strings:

| Exception             | Thrown When                                                             |
|------------------------|---------------------------------------------------------------------------|
| `EmailError`           | Generic/base failure — missing required fields, curl init failure, unmapped curl error codes |
| `SmtpConnectionError`  | libcurl can't reach the server (DNS resolution, TCP connect, or TLS handshake failure) |
| `SmtpAuthError`        | The server rejects the credentials (curl code `67`)                      |
| `AttachmentError`      | An attached file can't be read from disk                                  |

Every model implements `toString()` for easy logging:

```ez
try {
    client.send(msg)
} catch (e) {
    out e.toString()
    // e.g. "SmtpAuthError: Authentication failed for user 'me@gmail.com'. Check your password or App Password."
}
```

`mapCurlError()` in `errors.ez` is what translates raw curl integer codes into the right exception type after `curl_easy_perform` fails.

---

## Architecture

`main.ez` is the **public surface only**. All real work lives under `src/`, with a strict one-directional dependency chain and a single point of contact with the operating system:

```
                 ┌───────────────┐
                 │   main.ez     │   connect() / send_quick() / backend()
                 └───────┬───────┘
                         │ use
        ┌────────────────┼────────────────┬───────────────┐
        ▼                ▼                ▼               ▼
 src/client.ez     src/message.ez    src/mime.ez     src/errors.ez
        │                                 │               ▲
        │                                 └───────use──────┘
        │
        └──────────────use────────────────► src/ffi.ez
                                                   │
                                                   ▼
                                        libcurl-4.dll / libcurl.dll
                                              msvcrt.dll
```

- **`src/ffi.ez`** is the *only* file allowed to call `os_load_lib`, `os_get_func`, or `os_call`. If libcurl's C API shape ever changes, this is the only file that needs to change.
- **`src/mime.ez`** owns all payload string-building. It never touches curl or file I/O.
- **`src/client.ez`** owns the curl handle and temp-file lifecycle. It never touches payload formatting.
- **`src/message.ez`** is a pure data builder with no I/O and no FFI at all.
- **`src/errors.ez`** defines the exception hierarchy every other file throws into.

### Send Sequence

1. `main.connect()` constructs an `SmtpClient`.
2. `client.send(msg)` calls `msg.validate()`.
3. `mime.buildPayload(msg)` renders the RFC 2822 string.
4. The payload is written to a temp file (`ez_email_<clock>.tmp`).
5. A curl `easy` handle is initialized and configured: URL, auth, envelope `MAIL FROM` / `RCPT TO`, TLS options, upload source.
6. `curl_easy_perform` runs the SMTP conversation.
7. Any non-`CURLE_OK` result is mapped to a structured exception via `mapCurlError()`.
8. Cleanup always runs — file handle closed, recipient `slist` freed, curl handle cleaned up, temp file deleted — regardless of success or failure.
9. If an error occurred, it's re-thrown after cleanup; otherwise `true` is returned.

---

## File-by-File Reference

| File            | Responsibility |
|------------------|----------------|
| `main.ez`        | Public API: `connect()`, `send_quick()`, `backend()`. Imports and re-exports everything else. |
| `src/ffi.ez`     | Loads `libcurl-4.dll`/`libcurl.dll` and `msvcrt.dll`; exposes function tables (`__CurlFFI`, `__MsvcrtFFI`) and libcurl option/result constants. |
| `src/errors.ez`  | Exception models: `EmailError`, `SmtpConnectionError`, `SmtpAuthError`, `AttachmentError`; `mapCurlError()` translator. |
| `src/message.ez` | `Message` fluent builder model: recipients, subject, body, attachments, priority, custom headers, validation. |
| `src/mime.ez`    | `buildPayload()` — turns a `Message` into an RFC 2822 wire payload, including base64 attachment encoding and MIME boundaries. |
| `src/client.ez`  | `SmtpClient` model — TLS configuration, curl handle lifecycle, temp-file lifecycle, error mapping on send. |

---

## Resource Safety

`SmtpClient.send()` follows a manual "always cleanup" pattern rather than relying on `finally`, because **EZ does not run `finally` after `give`**. Instead:

- `curlHandle`, `rcptSlist`, and `fileHandle` are initialized to `0` before the `try` block.
- Any failure inside the `try` is caught and stored in `sendErr`, not re-thrown immediately.
- After the `try/catch`, cleanup runs unconditionally for every handle that was actually opened.
- The temp file is deleted unconditionally, on both the success and failure paths.
- Only after cleanup completes is `sendErr` re-thrown (if one occurred).

This guarantees no leaked curl handles, no leaked `slist` memory, and no orphaned temp files, even when a send fails partway through.

---

## MIME Payload Format

`buildPayload()` supports three content strategies depending on what's set on the `Message`:

| Scenario                     | `Content-Type`                              |
|-------------------------------|-----------------------------------------------|
| Plain text only               | `text/plain; charset=UTF-8`                   |
| HTML only, no attachments     | `text/html; charset=UTF-8`                    |
| Any attachment present        | `multipart/mixed; boundary="..."` — one text/html body part, plus one `application/octet-stream` part per attachment |

Additional details:

- **Date header** is built manually in RFC 2822 format (`Sat, 19 Jul 2026 17:05:50 +0500`) using locale-independent day/month name tables, since `strftime`'s `%a`/`%b` are locale-dependent.
- **Message-ID** is generated from a millisecond timestamp plus the sender's domain, to avoid deduplication by mail servers.
- **Attachments** are base64-encoded using EZ's built-in URL-safe `b64url_encode`, then converted to the standard base64 alphabet (`-`→`+`, `_`→`/`) with `=` padding added, and wrapped at 76 characters per line.
- **BCC** addresses are deliberately excluded from all headers — they only ever reach the server via the SMTP envelope (`RCPT TO`).

---

## Design Notes & Caveats

- **No C++.** The entire library calls the libcurl C API directly via EZ's FFI — there is no compiled extension to build or ship.
- **Windows-oriented FFI.** DLL names (`libcurl-4.dll`, `msvcrt.dll`) and the native CA store option assume a Windows target for this release.
- **Temp files.** Payloads are written to a temp file rather than streamed from memory, and read back via `msvcrt`'s `fopen`/`fread` through curl's `CURLOPT_READDATA`. The temp filename includes a clock-based value to reduce collision risk between concurrent sends.
- **Insecure mode.** Setting `client.verifyCert = false` disables both peer and hostname verification — use only against trusted infrastructure you control.
- **Priority values.** `priority()` only recognizes the literal strings `"high"`, `"normal"`, and `"low"`; any other value is silently ignored (no exception is thrown).

---

## License

MIT License