# httpx

An HTTP client for EZ that tells you what happened. Real status codes, real
response headers, any verb, byte-exact bodies.

```ez
use "httpx"

r = HTTP.get("https://api.example.com/users/1")

when r.status == 404 { give nil }
r.raiseForStatus()
give r.json()
```

## Install

```
ez install httpx
```

No dependencies. Windows only — it goes through WinHTTP, which ships with the
operating system.

## Why this exists

EZ's built-in `http_get` and `http_post` return the response body and nothing
else. The `http` package is built on them, and reports `statusCode = 200` for
every response — its own source says `// Native doesn't return status`.

So before this, nothing written in EZ could tell a 404 from a 200, read a
`Location` header, send a `PUT`, or download a file without corrupting it.

This goes through WinHTTP instead:

| | built-in / `http` | `httpx` |
|---|---|---|
| status code | always 200 | the real one |
| response headers | not available | all of them |
| verbs | GET, POST | any |
| binary bodies | corrupted | byte-exact |
| redirects | followed, invisibly | followed, or not, your choice |
| timeouts | fixed 30s | per client or per request |

## Making requests

```ez
HTTP.get(url)
HTTP.post(url, { "json": payload })
HTTP.put(url, { "body": raw })
HTTP.patch(url, { "form": { "name": "value" } })
HTTP.delete(url)
HTTP.head(url)
```

`HTTP` is a ready-made client. Build your own when you want settings:

```ez
api = Http()
    .withBaseUrl("https://api.example.com/v1")
    .withBearer(TOKEN)
    .withTimeout(10000)
    .withRetries(3)

user = api.get("/users/1").raiseForStatus().json()
```

Every `with…` returns the client, so they chain.

> `get` is a loop keyword in EZ, so a plain function named `get` can be defined
> but never called. As a method it is unambiguous — which is why the shorthand
> is an object rather than a set of functions.

### Options

| key | does |
|---|---|
| `json` | serialise as JSON and set the content type |
| `form` | url-encode and set the content type |
| `body` | send verbatim |
| `query` | append parameters to the URL |
| `headers` | for this request only |
| `timeoutMs` | for this request only |

Only one of `json` / `form` / `body` — passing two is an error rather than a
silent choice. A `Content-Type` you set yourself is never overwritten.

## The response

```ez
r.status            r.ok                 r.body
r.json()            r.jsonOr(fallback)   r.text()
r.header("Location")                     r.contentType()
r.isRedirect()      r.isClientError()    r.isServerError()
r.elapsedMs         r.contentLength()    r.bytes()
r.save(path)        r.raiseForStatus()
```

Header lookup is case-insensitive — the case a server picks is not something
you should have to know. Headers that legitimately repeat, like `Set-Cookie`,
come back as a list rather than being joined with a comma, which would corrupt
any cookie containing one.

`raiseForStatus()` puts the status *and the start of the body* in the message,
because the reason is almost always in the body:

```
httpx: POST https://api.example.com/users returned 422 Unprocessable Entity:
{"error":"email already registered"}
```

`json()` does the same when the body will not parse — usually because a proxy
returned an HTML error page, which is much easier to see than "invalid JSON".

## Binary

Bodies are byte-exact in both directions. `os_write_string` copies the whole
string with one memcpy on the way out and `os_buffer_from_ptr` copies back the
same way, so an image survives the round trip.

```ez
r = HTTP.get("https://example.com/logo.png")
r.save("logo.png")

download("https://example.com/archive.tgz", "archive.tgz")
```

The test suite downloads a real gzip file and checks it arrives at exactly its
published length with its magic bytes intact.

## Retries

```ez
client.withRetries(3, 200)                     # 3 attempts, growing delay
```

Retries apply to transport failures, `5xx` and `429` — the server saying "not
now". A `404` is an answer, not a failure; retrying it just makes four of them.

## Redirects and TLS

Redirects are followed by default. `withoutRedirects()` stops that, so you can
read the `Location` yourself.

Certificates are verified. `allowInvalidCertificates()` exists for a
development box with a self-signed certificate and should never be used for
anything that matters.

## URLs

```ez
use "httpx/url.ez"

parseUrl("https://user:pw@example.com:8443/a/b?x=1#top")
encodeQuery({ "tag": ["a", "b"], "n": 1 })     # "tag=a&tag=b&n=1"
decodeQuery("t=a&t=b")                         # { "t": ["a", "b"] }
addQuery(url, params)
buildUrl(parts)
```

Parsed in EZ rather than by WinHTTP, so the result is inspectable and a
malformed URL fails with something readable instead of a Windows error code.
IPv6 literals, credentials, and fragments are all handled — the fragment is a
browser concept and never reaches the server.

## Shorthands

```ez
getJson(url)                                   # fetch, check, parse
postJson(url, payload)
fetchUrl(url)
download(url, path)
```

## Errors

A transport failure throws with the reason in words, not a Windows error code:

```
httpx: could not connect to api.example.com:443 (the host name could not be resolved)
httpx: sending the request failed (a TLS error -- the certificate may be invalid or expired)
```

A response, on the other hand, is returned however bad its status is. Deciding
that a 404 is fatal is your call, not the client's — call `raiseForStatus()`
when it is.

## Tests

```
ez test.ez
```

147 assertions. The live section runs against the EZ package registry — a real
HTTPS server with a stable 200, a stable 404 and a binary file of known length.
If the network is down those are skipped and the skip is reported, rather than
passing quietly.

## License

MIT
