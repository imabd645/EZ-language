# EZ Database Library (EZDB)

A secure and powerful object-oriented SQLite wrapper for the EZ language.

## Features

- **Parameterized Queries**: Prevents SQL injection automatically.
- **Automated CRUD**: Pass dictionaries to `insert()` and `update()` to avoid writing manual SQL.
- **Transaction Support**: Native `begin()`, `commit()`, and `rollback()`.
- **OO Design**: Connection state managed inside the `Database` model.

## Installation

```bash
ez install db
```

## Usage

```ez
use "db"
```

## API Reference

### `Database` Model

#### `connect(path)`
Opens or creates a SQLite database at the specified path.

#### `query(sql, [params])`
Executes a SELECT query and returns an array of dictionaries (rows).

#### `execute(sql, [params])`
Executes a command (INSERT, UPDATE, DELETE).

#### `insert(table, data_dict)`
Inserts a record using a dictionary. Returns the last inserted ID.

#### `update(table, id, data_dict)`
Updates a record by ID using a dictionary.

#### `delete(table, id)`
Deletes a record by ID.

#### `begin()` / `commit()` / `rollback()`
Standard transaction management.

## Examples

```ez
use "db"

db = openDatabase("my_app.db")

// Schema
db.createTable("tasks", ["id INTEGER PRIMARY KEY", "title TEXT", "done BOOL"])

// Secure Insert
id = db.insert("tasks", {title: "Write README", done: false})

// Parameterized Query
results = db.query("SELECT * FROM tasks WHERE done = ?", [false])
get t in results {
    out t.title
}

db.disconnect()
```
