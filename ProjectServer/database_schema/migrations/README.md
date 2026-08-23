# Database Migrations

This directory contains incremental, idempotent SQL migrations for the
SquirrelCommunicator database.

The `database_schema/*.sql` files are the **full schema snapshots** used to
create a database from scratch (fresh installs / test environments). The files
in this `migrations/` directory are the **incremental deltas** applied to
existing databases that were created from an older snapshot.

## Naming

```
NNNN_short_description.sql
```

- `NNNN` is a zero-padded, monotonically increasing sequence number.
- `short_description` is a concise, snake_case summary of the change.

## Applying migrations

Run each numbered file **in ascending order** against the target database:

```bash
mysql -h <host> -u <user> -p <database> < migrations/0001_add_message_type.sql
```

All migrations are written to be **idempotent** (safe to re-run) using
`ADD COLUMN IF NOT EXISTS`, `CREATE TABLE IF NOT EXISTS`, etc., where the
database engine supports it. The server is MariaDB 11.x, which supports these
clauses.

## Current migrations

| File | Description |
|------|-------------|
| `0001_add_message_type.sql` | Adds a `message_type` TINYINT discriminator (0=text, 1=image, 2=gif, 3=video) to both `messages` and `server_messages`. Media messages store the verified SHA-256 content hash in the existing `text`/`content` column. |
