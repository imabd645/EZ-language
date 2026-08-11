# Expense Tracker

A complete web application written in EZ: accounts, persistence, filtering,
reports, a CSV export and a JSON API.

```
ez seed.ez        # create the demo account and sample data
ez app.ez         # http://localhost:8080
ez test_app.ez    # 85 tests against the data layer, no server needed
```

Sign in with `demo@example.com` / `demo1234`.

## What it does

- **Accounts** — registration, sign-in, password change. Passwords are stored as
  PBKDF2 hashes (600 000 iterations, per-user salt) via the `auth` package.
- **Expenses** — create, edit, delete, with server-side validation of amount,
  category and date.
- **Filtering** — by category, date range and description search, combinable.
- **Reports** — totals by category and by month, with proportion bars.
- **CSV export** — the current view, quoted per RFC 4180.
- **JSON API** — the same data behind a JWT bearer token.

## Layout

```
app.ez              routes, session handling, API
seed.ez             demo data
test_app.ez         data-layer test suite
src/db.ez           connection, schema, versioned migrations
src/users.ez        registration, authentication
src/expenses.ez     CRUD, queries, reports, CSV import
src/views.ez        HTML rendering
```

`src/` holds everything that does not need a web server, which is why the test
suite can cover the whole data layer without starting one.

## Data model

Two tables, created by `runMigrations()` and versioned with `PRAGMA
user_version` so an existing database upgrades in place.

```sql
users     id, email (unique, case-insensitive), name, password_hash, created_at
expenses  id, user_id -> users(id) ON DELETE CASCADE,
          amount, category, description, spent_on, created_at
```

Indexed on `(user_id, spent_on DESC)` — the shape of every listing query — and on
`(user_id, category)`.

`PRAGMA foreign_keys=ON` is set on every connection. It is **off** by default in
SQLite, and without it the `ON DELETE CASCADE` above is silently ignored, so
deleting a user would leave their expenses behind.

## Security notes

Things that are easy to get wrong and are handled deliberately:

- **Every expense query is scoped by `user_id`.** That is the whole
  authorisation model, so it lives in `src/expenses.ez` rather than being left
  to each route to remember. One user asking for another's row gets `nil`, not
  someone else's data. Covered by tests.
- **All SQL uses bound parameters.** Filter values arrive straight from a query
  string; a description of `'; DROP TABLE expenses; --` is stored as text and the
  table survives. Covered by a test.
- **Everything user-supplied is HTML-escaped** on the way out, so a description
  cannot inject a `<script>` into another page.
- **A failed login never says which half was wrong.** "No such account" and
  "wrong password" are the same message, and `authenticate()` hashes a dummy
  password when the email is unknown so a missing account does not answer
  measurably faster.
- **The session cookie holds only the user id.** The row is re-read each request,
  so a deleted or renamed account takes effect immediately.

## API

```bash
TOKEN=$(curl -s -X POST localhost:8080/api/login \
  -H 'Content-Type: application/json' \
  -d '{"email":"demo@example.com","password":"demo1234"}' \
  | grep -o '"token": "[^"]*"' | cut -d'"' -f4)

curl -s localhost:8080/api/me            -H "Authorization: Bearer $TOKEN"
curl -s localhost:8080/api/expenses      -H "Authorization: Bearer $TOKEN"
curl -s localhost:8080/api/summary       -H "Authorization: Bearer $TOKEN"

curl -s -X POST localhost:8080/api/expenses -H "Authorization: Bearer $TOKEN" \
  -H 'Content-Type: application/json' \
  -d '{"amount":12.5,"category":"Food","description":"Lunch","spent_on":"2026-08-11"}'

curl -s -X DELETE localhost:8080/api/expenses/1 -H "Authorization: Bearer $TOKEN"
```

Tokens expire after an hour. `/health` needs no token.

## Notes for anyone reading the source

Three things about EZ that shaped this code:

- **`csv` and `json` both export `parse`, `stringify`, `read` and `write`.** They
  are imported as `use "csv" as csv` / `use "json" as json`, because without the
  alias whichever `use` came last silently wins and `csv.stringify(rows, delim,
  columns)` would land on `json.stringify(val)`.

- **`get`, `other` and `async` are keywords** and cannot be used as names. That is
  why the JSON accessors are `getPath`/`setPath`/`hasPath` and why this code says
  `bob` rather than `other`.

- **A bare assignment inside a task writes to an existing outer global.** The
  local database handle is a module-level `_dbHandle` with an underscore for that
  reason: a library function that assigns to a plain `db` would otherwise
  overwrite a caller's `db` — which is exactly what `sqlite.connect()` used to do
  before it was changed to declare its local with a type.

## Deployment

`SESSION_SECRET` and `JWT_SECRET` in `app.ez` are development defaults. Replace
them before running this anywhere real: anyone who knows the JWT secret can mint
a token for any user.
