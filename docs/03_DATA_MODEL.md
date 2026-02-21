<!-- markdownlint-disable MD013 MD022 MD032 -->
# 03 Data Model

Phase-0 SQLite schema creates table `app_meta`:

- `k TEXT PRIMARY KEY`
- `v TEXT NOT NULL`

The initial row records schema version:

- `('schema_version', '1')`

Future mailbox/message tables are deferred to later phases.
