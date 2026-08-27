# crypto — Cross-Platform Cryptographic Suite for EZ

> **Version:** 2.0.0  
> **Import:** `use "crypto"`  
> **Path:** `C:\ezlib\crypto\main.ez`  
> **Compatibility:** Windows, Linux, macOS (Cross-Platform Dynamic FFI + Fallback)

---

## Overview

`crypto` provides a robust, modular, and cross-platform suite of cryptographic primitives and utilities for EZ:

- **SecureRandom**: Cryptographically secure pseudo-random number generator (CSPRNG), UUID v4, random ints/floats, and secure tokens.
- **Hash**: SHA-256, SHA-512, SHA-384, SHA-1, and MD5 digests.
- **HMAC**: RFC 2104 / RFC 4231 Keyed-Hash Message Authentication Codes.
- **AES**: AES-128, AES-192, and AES-256 in CBC mode with PKCS#7 padding.
- **PBKDF2**: RFC 2898 / RFC 6070 Password-Based Key Derivation Function 2.
- **Base64 / Hex**: Standard Base64, URL-Safe Base64 (RFC 4648), Hex encoding/decoding, and constant-time equality comparisons.
- **Crypto**: Unified static facade for common operations.

```
crypto/
  package.ez        ← manifest
  main.ez           ← public facade
  test_crypto.ez    ← comprehensive test suite
  src/
    ffi.ez          ← dynamic cross-platform native loader
    hex.ez          ← hex encoding and timing-safe equality
    base64.ez       ← standard & URL-safe Base64
    random.ez       ← CSPRNG & UUID v4
    hash.ez         ← SHA-256/512/384/1, MD5
    hmac.ez         ← RFC 2104 / 4231 HMAC
    cipher.ez       ← AES-CBC & PKCS#7
    pbkdf2.ez       ← RFC 2898 PBKDF2
```

---

## Quick Start

```ez
use "crypto"

# 1. Hashing
digest = Hash.sha256("password123")
out "SHA-256: " + digest

# 2. HMAC-SHA256
sig = HMAC.sha256("secretKey", "payloadData")
out "HMAC: " + sig

# 3. AES-256-CBC Encryption
key = "0123456789abcdef0123456789abcdef"  # 32 bytes
iv  = "1234567890abcdef"                  # 16 bytes
cipher = AES.encrypt(key, iv, "Secret Message")
plain  = AES.decrypt(key, iv, cipher)
out "Decrypted: " + plain

# 4. UUID v4 & Secure Random
userId = SecureRandom.uuid4()
token  = SecureRandom.token(32)
out "User ID: " + userId
out "Token: " + token
```

---

## API Reference

### `Hash`
- `Hash.sha256(data) -> string`
- `Hash.sha512(data) -> string`
- `Hash.sha384(data) -> string`
- `Hash.sha1(data) -> string`
- `Hash.md5(data) -> string`
- `Hash.sha256Bytes(data) -> array`

### `HMAC`
- `HMAC.sha256(key, message) -> string`
- `HMAC.sha512(key, message) -> string`
- `HMAC.sha384(key, message) -> string`
- `HMAC.sha1(key, message) -> string`
- `HMAC.md5(key, message) -> string`

### `AES` & `PKCS7`
- `AES.encrypt(key, iv, plaintext) -> string (Base64)`
- `AES.decrypt(key, iv, cipherBase64) -> string (Plaintext)`
- `PKCS7.pad(data, blockSize) -> array`
- `PKCS7.unpad(data) -> array`

### `PBKDF2`
- `PBKDF2.derive(password, salt, iterations = 100000, keyLen = 32) -> string (Hex)`
- `PBKDF2.deriveBytes(password, salt, iterations, keyLen) -> array`

### `SecureRandom`
- `SecureRandom.bytes(count) -> array`
- `SecureRandom.randInt(min, max) -> int`
- `SecureRandom.randFloat() -> float`
- `SecureRandom.hex(numBytes) -> string`
- `SecureRandom.token(numBytes) -> string`
- `SecureRandom.uuid4() -> string`

### `Base64` & `Hex`
- `Base64.encode(input) -> string`
- `Base64.decode(encoded) -> string`
- `Base64.urlEncode(input) -> string`
- `Base64.urlDecode(encoded) -> string`
- `Hex.encode(input) -> string`
- `Hex.decode(hexStr) -> string`
- `Hex.timingSafeEqual(a, b) -> bool`
