# crypto — Cryptography Library for EZ

> **Version:** 1.0  
> **Import:** `use "crypto"`  
> **File:** `C:\ezlib\crypto\main.ez`

---

## Overview

`crypto` provides cryptographic primitives implemented in pure EZ using the language's native bitwise operators. It includes:

- **Base64** encoding and decoding (RFC 4648 compliant)
- **RC4** stream cipher (symmetric encryption/decryption)
- **Crypto** facade model (convenient static interface)

All operations are done entirely in EZ — no external DLLs or OS calls are needed.

---

## Quick Start

```ez
use "crypto"

# Base64 encode
encoded = Crypto.base64Encode("Hello, World!")
out encoded   # → "SGVsbG8sIFdvcmxkIQ=="

# Base64 decode
decoded = Crypto.base64Decode("SGVsbG8sIFdvcmxkIQ==")
out decoded   # → "Hello, World!"

# RC4 encrypt
key = "mySecretKey"
cipherText = Crypto.rc4Encrypt(key, "Secret message")
out cipherText  # → Base64-encoded cipher

# RC4 decrypt
plainText = Crypto.rc4Decrypt(key, cipherText)
out plainText   # → "Secret message"
```

---

## Model: `Base64`

The `Base64` model provides static methods for encoding and decoding data using the standard Base64 alphabet (`A-Z`, `a-z`, `0-9`, `+`, `/`).

### `Base64.encode(input)` → `string`

Encodes a string or byte array to Base64.

**Parameters:**
- `input` — A `string` or an array of byte integers (`0–255`).

**Returns:** Base64-encoded string, padded with `=` as required.

**Examples:**
```ez
use "crypto"

# String input
out Base64.encode("Hello")      # → "SGVsbG8="
out Base64.encode("Hi")         # → "SGk="
out Base64.encode("A")          # → "QQ=="
out Base64.encode("")           # → ""

# Byte array input
out Base64.encode([72, 101, 108, 108, 111])  # → "SGVsbG8="

# Padding cases
out Base64.encode("Man")        # → "TWFu"    (no padding needed — 3 bytes)
out Base64.encode("Ma")         # → "TWE="    (1 pad char)
out Base64.encode("M")          # → "TQ=="    (2 pad chars)
```

---

### `Base64.decode(encoded)` → `string`

Decodes a Base64-encoded string back to a plain text string.

**Parameters:**
- `encoded` — A valid Base64 string (may include `=` padding).

**Returns:** Decoded string, built by converting each byte back to a character via `chr()`.

**Examples:**
```ez
use "crypto"

out Base64.decode("SGVsbG8=")       # → "Hello"
out Base64.decode("SGk=")           # → "Hi"
out Base64.decode("QQ==")           # → "A"
out Base64.decode("")               # → ""
out Base64.decode("SGVsbG8sIFdvcmxkIQ==")  # → "Hello, World!"
```

> ⚠️ **Note:** `Base64.decode()` assumes the encoded content represents printable ASCII text. For binary data (images, files, etc.), use `Base64.decodeToBytes()` instead.

---

### `Base64.decodeToBytes(encoded)` → `array`

Decodes a Base64 string to a raw byte array (array of integers `0–255`).

**Parameters:**
- `encoded` — A valid Base64 string.

**Returns:** Array of byte integers.

**Examples:**
```ez
use "crypto"

bytes = Base64.decodeToBytes("SGVsbG8=")
out bytes     # → [72, 101, 108, 108, 111]
out bytes[0]  # → 72 (ASCII 'H')
```

Use this when you need to work with binary data, BLOB content, or pipe decoded bytes into another operation.

---

## Model: `RC4`

RC4 (Rivest Cipher 4) is a symmetric stream cipher. The same operation encrypts and decrypts — just apply with the same key.

> ⚠️ **Security Warning:** RC4 has known cryptographic weaknesses and should **not** be used for high-security applications. It is suitable for obfuscation, lightweight encoding, or educational purposes.

### `RC4.init(key)` → `RC4 instance`

Creates and initializes an RC4 cipher instance with the given key.

- The key is used to initialize a 256-byte S-box via the KSA (Key Scheduling Algorithm).
- The key can be any non-empty string of any length.

```ez
use "crypto"

cipher = RC4("my-secret-key-123")
```

### `RC4.processBytes(input)` → `array`

XORs the input with the RC4 keystream. Can be called with either a string or a byte array.

**Parameters:**
- `input` — A `string` or array of byte integers.

**Returns:** Array of byte integers (the cipher/plain text bytes).

**Important:** Each `RC4` instance maintains state. After calling `processBytes()`, the internal i/j counters advance. To decrypt the same ciphertext, create a **fresh `RC4` instance** with the same key.

**Examples:**
```ez
use "crypto"

key = "password"
plain = "Attack at dawn"

# Encrypt
encCipher = RC4(key)
encBytes = encCipher.processBytes(plain)
out encBytes   # → array of XOR'd bytes

# Decrypt (must use a NEW RC4 instance)
decCipher = RC4(key)
decBytes = decCipher.processBytes(encBytes)

result = ""
repeat i = 0 to len(decBytes) - 1 {
    result = result + chr(decBytes[i])
}
out result   # → "Attack at dawn"
```

---

## Model: `Crypto`

The `Crypto` facade provides convenient static methods combining `Base64` and `RC4`.

### `Crypto.base64Encode(text)` → `string`

Shorthand for `Base64.encode(text)`.

```ez
use "crypto"

out Crypto.base64Encode("EZ Language!")  # → "RVogTGFuZ3VhZ2Uh"
```

---

### `Crypto.base64Decode(text)` → `string`

Shorthand for `Base64.decode(text)`.

```ez
use "crypto"

out Crypto.base64Decode("RVogTGFuZ3VhZ2Uh")  # → "EZ Language!"
```

---

### `Crypto.rc4Encrypt(key, text)` → `string`

Encrypts plaintext with RC4 and returns a **Base64-encoded** cipher string (safe for storage and transmission).

**Parameters:**
- `key` — Encryption key string.
- `text` — Plaintext to encrypt.

**Returns:** Base64-encoded ciphertext string.

```ez
use "crypto"

cipher = Crypto.rc4Encrypt("SecretKey", "Hello World")
out cipher   # → Base64 string, e.g. "GvY3Sl/T8OaG"
```

---

### `Crypto.rc4Decrypt(key, base64Text)` → `string`

Decrypts an RC4-encrypted, Base64-encoded string.

**Parameters:**
- `key` — The same key used during encryption.
- `base64Text` — The Base64-encoded ciphertext from `rc4Encrypt()`.

**Returns:** Decrypted plaintext string.

```ez
use "crypto"

key = "SecretKey"
cipher = Crypto.rc4Encrypt(key, "My secret data")
plain = Crypto.rc4Decrypt(key, cipher)
out plain   # → "My secret data"
```

---

## Edge Cases & Important Notes

### Empty Inputs
```ez
use "crypto"

out Base64.encode("")        # → ""
out Base64.decode("")        # → ""
out Base64.decodeToBytes("") # → []

# RC4 with empty string
cipher = RC4("key")
out cipher.processBytes("")  # → []

# rc4Decrypt with empty cipher
out Crypto.rc4Decrypt("key", "")  # → ""
```

### Key Length for RC4
RC4's KSA uses `key[i % keyLen]`, so any key length from 1 to 256 bytes is valid. Longer keys (40+ chars) provide better security, though RC4 itself has fundamental vulnerabilities regardless.

### RC4 State is Consumed
After calling `processBytes()`, the cipher state advances. **Never reuse the same RC4 instance** for a second message:
```ez
# WRONG — decryption will fail
cipher = RC4("key")
enc = cipher.processBytes("message1")
dec = cipher.processBytes(enc)    # ❌ Wrong! State is already advanced
out dec  # → garbage

# CORRECT — create fresh instances
enc = RC4("key").processBytes("message1")
dec = RC4("key").processBytes(enc)  # ✅ Fresh instance
```

### Binary Data
If encoding binary data (non-text), use `Base64.decodeToBytes()` and work with byte arrays directly. `Base64.decode()` calls `chr()` on each byte, which only works correctly for ASCII range (0-127).

### Non-ASCII Characters
`Base64.encode()` uses `ord()` on each character, which works correctly for ASCII. For multibyte Unicode strings, `ord()` returns the code point of the first byte only — behavior may be unexpected for non-ASCII input.

### Invalid Base64 Input
If the input to `Base64.decodeToBytes()` or `Base64.decode()` contains characters not in the Base64 alphabet, `indexOf()` returns `-1` which is treated as index `0`. This means corrupted/invalid Base64 data will silently produce wrong output rather than an error.

---

## Full Example: Config File Obfuscation

```ez
use "crypto"

APP_KEY = "MyAppSecretKey2024"

# Obfuscate sensitive config before saving
task saveConfig(data) {
    json = to_json(data)
    encrypted = Crypto.rc4Encrypt(APP_KEY, json)
    writeFile("config.dat", encrypted)
    out "Config saved (encrypted)."
}

# Load and decrypt config
task loadConfig() {
    raw = readFile("config.dat")
    json = Crypto.rc4Decrypt(APP_KEY, raw)
    give parse_json(json)
}

# Save config
config = {
    "db_host": "localhost",
    "db_pass": "supersecret",
    "api_key": "key-abc123"
}
saveConfig(config)

# Load it back
loaded = loadConfig()
out loaded["db_host"]  # → "localhost"
out loaded["api_key"]  # → "key-abc123"
```

---

## Full Example: Token System

```ez
use "crypto"

SECRET = "server-signing-key"

# Create a token embedding user ID and expiry
task createToken(userId, expiryMs) {
    data = str(userId) + ":" + str(expiryMs)
    give Crypto.rc4Encrypt(SECRET, data)
}

# Validate and parse a token
task validateToken(token) {
    data = Crypto.rc4Decrypt(SECRET, token)
    parts = split(data, ":")
    
    when len(parts) != 2 {
        give nil   # Invalid format
    }
    
    userId = num(parts[0])
    expiry = num(parts[1])
    
    when clock() > expiry {
        give nil   # Token expired
    }
    
    give userId
}

# Create a token that expires in 1 hour
expiryTime = clock() + 3600000
token = createToken(42, expiryTime)
out "Token: " + token

# Validate
userId = validateToken(token)
when userId {
    out "Valid! User ID: " + str(userId)
} other {
    out "Invalid or expired token."
}
```

---

## Full Example: Password Hashing Simulation

```ez
use "crypto"

# Simple (non-cryptographic) password storage simulation
task hashPassword(password, salt) {
    combined = salt + password
    give Crypto.base64Encode(Crypto.rc4Encrypt("static-server-key", combined))
}

task verifyPassword(password, salt, storedHash) {
    give hashPassword(password, salt) == storedHash
}

salt = "random-salt-abc"
stored = hashPassword("myPassword123", salt)
out "Stored: " + stored

out verifyPassword("myPassword123", salt, stored)  # → true
out verifyPassword("wrongPassword", salt, stored)   # → false
```

---

## Algorithm Details

### Base64 Encoding
- Processes input in 3-byte groups
- Converts each 3 bytes to 4 Base64 characters using 6-bit shifts
- Pads with `=` if input length is not divisible by 3
- Padding: `rem == 1` → `==`, `rem == 2` → `=`

### RC4 Key Scheduling Algorithm (KSA)
```
Initialize S = [0, 1, 2, ..., 255]
j = 0
for i = 0 to 255:
    j = (j + S[i] + key[i % keyLen]) mod 256
    swap(S[i], S[j])
```

### RC4 Pseudo-Random Generation Algorithm (PRGA)
```
i = 0, j = 0
for each byte of input:
    i = (i + 1) mod 256
    j = (j + S[i]) mod 256
    swap(S[i], S[j])
    K = S[(S[i] + S[j]) mod 256]
    output = input_byte XOR K
```

---

*Documentation generated from `C:\ezlib\crypto\main.ez` — EZ Crypto Library*
