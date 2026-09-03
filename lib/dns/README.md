# `dns` — DNS Resolver & Domain Security Inspector for EZ

The `dns` package provides high-performance, encrypted **DNS-over-HTTPS (DoH)** record resolution, reverse DNS lookups, custom resolver configuration, and domain security verification (SPF, DMARC, DKIM, and multi-provider propagation checks) for the EZ programming language using `httpx`.

---

## Quick Start

```ez
use "dns" as dns

# 1. Resolve IPv4 and IPv6
ips = dns.resolve4("google.com") # ["142.250.190.46", ...]
ips6 = dns.resolve6("google.com") # ["2607:f8b0:4005:805::200e", ...]

# 2. Inspect Mail Servers (MX)
mailServers = dns.resolveMx("google.com")
get mx in mailServers {
    out mx.exchange + " (priority: " + str(mx.priority) + ")"
}

# 3. Check Domain Security & SPF/DMARC Configuration
spf = dns.getSpf("google.com")
out "SPF Policy: " + spf["policy"] # "~all"

dmarc = dns.getDmarc("google.com")
out "DMARC Policy: " + dmarc["policy"] # "reject"

# 4. Reverse DNS (PTR)
hosts = dns.reverse("8.8.8.8") # ["dns.google"]
```

---

## Table of Contents
1. [Core Functions](#1-core-functions)
2. [Supported Record Types](#2-supported-record-types)
3. [Configuring Custom Resolvers (`Resolver`)](#3-configuring-custom-resolvers-resolver)
4. [Security & Verification Helpers (`getSpf`, `getDmarc`, `getDkim`)](#4-security--verification-helpers)
5. [Worldwide DNS Propagation Checking](#5-worldwide-dns-propagation-checking)
6. [API Reference](#6-api-reference)

---

## 1. Core Functions

All standard queries can be called directly on the `dns` namespace:

| Function | Returns | Description |
| :--- | :--- | :--- |
| `dns.resolve4(domain)` | `array[string]` | Resolves IPv4 addresses (`A` records). |
| `dns.resolve6(domain)` | `array[string]` | Resolves IPv6 addresses (`AAAA` records). |
| `dns.lookup(domain)` | `string` | Returns the primary resolved IP address (IPv4 or IPv6). |
| `dns.resolveMx(domain)` | `array[MXRecord]` | Resolves mail exchange servers sorted by priority ascending. |
| `dns.resolveTxt(domain)` | `array[string]` | Resolves unquoted text records. |
| `dns.resolveNs(domain)` | `array[string]` | Resolves authoritative nameserver hostnames. |
| `dns.resolveCname(domain)` | `string` / `nil` | Resolves canonical name alias. |
| `dns.resolveSoa(domain)` | `SOARecord` / `nil` | Resolves Start of Authority record. |
| `dns.resolveSrv(domain)` | `array[SRVRecord]` | Resolves service locator records (`target`, `port`, `priority`, `weight`). |
| `dns.resolveCaa(domain)` | `array[CAARecord]` | Resolves Certification Authority Authorization records. |
| `dns.reverse(ip)` | `array[string]` | Performs reverse DNS lookup (`PTR`) to find hostnames for an IP. |

---

## 2. Supported Record Types

The library returns typed record models that inherit from `DnsRecord`:

### `ARecord` & `AAAARecord`
* `r.name` — Domain name (e.g. `"google.com"`)
* `r.ip` — IP address string (e.g. `"142.250.190.46"`)
* `r.ttl` — Time to live in seconds

### `MXRecord`
* `r.exchange` — Mail server host (e.g. `"smtp.google.com"`)
* `r.priority` — Preference number (e.g. `10`)

### `TXTRecord`
* `r.text` — Cleaned/unquoted text string (e.g. `"v=spf1 include:_spf.google.com ~all"`)

### `NSRecord`
* `r.host` — Nameserver host (e.g. `"ns1.google.com"`)

### `SOARecord`
* `r.primaryNs` — Primary nameserver
* `r.adminEmail` — Administrator mailbox
* `r.serial` — Zone serial version number
* `r.refresh` / `r.retry` / `r.expire` / `r.minimumTtl` — Zone timing intervals

---

## 3. Configuring Custom Resolvers (`Resolver`)

Create isolated `Resolver` instances with custom providers, timeouts, and retries:

```ez
use "dns" as dns

# Use Google DoH with 3 second timeout
googleResolver = dns.Resolver({
    "provider": "google",
    "timeout": 3000,
    "retries": 3
})

# Use AdGuard DoH (ad & tracking filtering DNS)
adguardResolver = dns.Resolver({
    "provider": "adguard",
    "timeout": 5000
})

# Use Custom DoH Endpoint
corporateResolver = dns.Resolver({
    "provider": "https://dns.mycompany.internal/dns-query"
})

ips = googleResolver.resolve4("github.com")
```

### Supported Built-in Providers:
* `"cloudflare"` — `https://cloudflare-dns.com/dns-query` (Default)
* `"google"` — `https://dns.google/resolve`
* `"adguard"` — `https://dns.adguard-dns.com/resolve`
* `"alidns"` — `https://dns.alidns.com/resolve`

---

## 4. Security & Verification Helpers

The `dns` module includes built-in auditing utilities for email security and domain misconfigurations:

### SPF Validation (`dns.getSpf(domain)`)
```ez
spf = dns.getSpf("google.com")
# Returns:
# {
#   "configured": true,
#   "raw": "v=spf1 include:_spf.google.com ~all",
#   "includes": ["_spf.google.com"],
#   "ip4": [],
#   "ip6": [],
#   "policy": "~all"
# }
```

### DMARC Policy Inspection (`dns.getDmarc(domain)`)
```ez
dmarc = dns.getDmarc("github.com")
# Returns:
# {
#   "configured": true,
#   "raw": "v=DMARC1; p=reject; rua=mailto:...",
#   "policy": "reject",
#   "pct": 100,
#   "rua": "mailto:..."
# }
```

### DKIM Record Query (`dns.getDkim(domain, selector)`)
```ez
dkim = dns.getDkim("example.com", "google")
out "DKIM Public Key: " + str(dkim["publicKey"])
```

### Domain Configuration Check (`dns.isConfigured(domain)`)
```ez
when not dns.isConfigured("unregistered-xyz123.com") {
    out "Domain is inactive or has no DNS records configured."
}
```

---

## 5. Worldwide DNS Propagation Checking

Query multiple global DoH providers in parallel to verify DNS record changes and propagation:

```ez
use "dns" as dns

results = dns.checkPropagation("example.com", "A")

get provider in keys(results) {
    status = results[provider]
    when status["ok"] {
        out provider + " -> " + str(status["count"]) + " records found"
    } other {
        out provider + " -> Query Failed: " + status["error"]
    }
}
```
