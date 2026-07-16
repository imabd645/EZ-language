# EZ Email Package (`email`)

A lightweight, robust SMTP and SMTPS email client library for the EZ Programming Language. It uses direct FFI bindings to `libcurl-4.dll` and `msvcrt.dll` for secure communication, supporting authentication, SSL/TLS, and custom SMTP servers like Gmail.

---

## Installation
If using the EZ Package Manager:
```bash
ez install email
```

---

## Quick Start (Gmail SMTPS Example)

```ez
use "lib/email.ez"

# 1. Initialize the SMTP Client
# SmtpClient(smtp_server, port, username, password)
client = SmtpClient("smtp.gmail.com", 587, "yourname@gmail.com", "your-google-app-password")

# 2. Configure options
client.verbose = true   # Set to true to see connection diagnostic logs
client.isHtml = true    # Set to true to send HTML styled emails

# 3. Send Email
# send(fromEmail, toEmail, subject, body)
client.send(
    "yourname@gmail.com", 
    "recipient@example.com", 
    "Hello from EZ!", 
    "<h1>Welcome!</h1><p>Sent natively via EZ FFI SMTP!</p>"
)

out "Email sent successfully!"
```

---

## API Reference

### `SmtpClient` Model

#### Properties
* **`server`** (string): The domain of your SMTP server (e.g. `"smtp.gmail.com"`).
* **`port`** (integer): The connection port. Typically `587` (STARTTLS) or `465` (SMTPS).
* **`username`** (string): Authentication username/email address.
* **`password`** (string): Authentication password or Google App Password (without spaces).
* **`useSsl`** (boolean, default: `true`): Enables SSL/TLS upgrades.
* **`verbose`** (boolean, default: `false`): Enables libcurl's deep connection trace logging on stdout.
* **`isHtml`** (boolean, default: `false`): Sets the content type to `text/html`. If false, defaults to `text/plain`.
* **`replyTo`** (string, default: `""`): Sets a custom reply-to address (e.g. `support@yourdomain.com`).
* **`priority`** (string, default: `""`): Sets priority value (e.g. `"1"` for High, `"3"` for Normal, `"5"` for Low). Adds `X-Priority` and standard `Importance` headers.
* **`customHeaders`** (dictionary, default: `{}`): A key-value dictionary of custom MIME headers to include in the email (e.g., `{"X-Mailer": "EZ SMTP"}`).

#### Methods
* **`init(server, port, username, password)`**: Instantiates a new SMTP Client.
* **`send(fromEmail, toEmail, subject, body)`**: Connects, authenticates, and uploads the email payload. Throws a runtime exception if sending fails.

---

## Gmail App Passwords Guide
To use Google's SMTP servers securely with this library:
1. Go to your **Google Account** settings.
2. Ensure **2-Step Verification** is active, then search for **App Passwords**.
3. Create a new App Password for "Other (Custom name)".
4. Google will generate a 16-character code (e.g. `puvg ndko anmm yigt`).
5. **CRITICAL**: Copy the password **WITHOUT SPACES** (`puvgndkoanmmyigt`) into your client password property.
