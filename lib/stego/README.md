# `stego` — Image Steganography for EZ

Hide and extract secret text, credentials, keys, or entire files inside BMP image pixels using **Least Significant Bit (LSB)** manipulation without visibly altering the image. Includes optional password-based stream cipher encryption with checksum integrity verification.

---

## Installation & Usage

```ez
use "stego" as stego
```

---

## 1. Quick Start: Hide & Reveal Secret Messages

```ez
use "stego" as stego

# 1. Create or use an existing BMP carrier image (300x200)
stego.createCarrier("carrier.bmp", 300, 200, "#4682B4")

# 2. Hide a secret message inside the image
stego.encode("carrier.bmp", "secret_image.bmp", "Super secret API key: ez_live_998822")

# 3. Extract the hidden message
hiddenMsg = stego.decode("secret_image.bmp")
out "Extracted Message: " + hiddenMsg
```

---

## 2. Password Encryption & Decryption

Protect your hidden messages with a password key. If someone attempts to decode without the password or provides the wrong password, extraction is rejected.

```ez
use "stego" as stego

# Encode with password
stego.encode("carrier.bmp", "secure_stego.bmp", "Bank Pin: 9876", "mySecretPasscode123")

# Decode with password
revealed = stego.decode("secure_stego.bmp", "mySecretPasscode123")
out "Revealed: " + revealed

# Decoding with wrong password throws an error
try {
    stego.decode("secure_stego.bmp", "wrongPassword")
} catch err {
    out "Caught expected error: " + str(err)
}
```

---

## 3. Image Capacity & Inspection

```ez
use "stego" as stego

# Check if image contains hidden data
when stego.hasSecret("photo.bmp") {
    out "This image has a hidden steganography payload!"
}

# Check maximum secret byte capacity
info = stego.capacity("carrier.bmp")
out "Dimensions:   " + str(info["width"]) + "x" + str(info["height"])
out "Max Capacity: " + str(info["maxSecretBytes"]) + " bytes (" + str(floor(info["maxSecretBytes"] / 1024)) + " KB)"
```

---

## 4. Hiding Entire Files

```ez
use "stego" as stego

# Hide a private file inside a BMP image
stego.encodeFile("carrier.bmp", "embedded_image.bmp", "passwords.txt", "vaultKey")

# Extract the file back
stego.decodeFile("embedded_image.bmp", "recovered_passwords.txt", "vaultKey")
```

---

## API Summary

| Function | Parameters | Description |
| :--- | :--- | :--- |
| `createCarrier(outPath, w, h, color)` | `outPath, w=300, h=200, color="#4682B4"` | Generates a clean carrier BMP image. |
| `encode(inBmp, outBmp, text, pass)` | `inBmp, outBmp, text, pass=nil` | Embeds secret text into image with LSB. |
| `decode(bmpPath, pass)` | `bmpPath, pass=nil` | Extracts hidden secret text from image. |
| `encodeFile(inBmp, outBmp, file, pass)`| `inBmp, outBmp, file, pass=nil` | Embeds an entire file into image. |
| `decodeFile(bmpPath, outFile, pass)` | `bmpPath, outFile, pass=nil` | Extracts embedded file to output path. |
| `capacity(bmpPath)` | `bmpPath` | Returns image dimensions & max capacity. |
| `hasSecret(bmpPath)` | `bmpPath` | Returns `true` if image contains `EZST` payload. |
