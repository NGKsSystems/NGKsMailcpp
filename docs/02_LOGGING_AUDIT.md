<!-- markdownlint-disable MD013 MD022 MD032 -->
# 02 Logging Audit

Audit output file: `artifacts/logs/audit.jsonl`.

Each JSON line contains:

- `ts` (UTC ISO-8601),
- `event` (for Phase 0: `APP_START`),
- `payload` (JSON object),
- `prev_hash` (hash of previous line),
- `hash` (SHA-256 of current event material).

On startup, the logger loads the last line hash and chains new events from it.
