<!-- markdownlint-disable MD013 MD022 MD032 -->
# 00 Scope

Phase 0 delivers a bootable Qt6/C++ desktop skeleton for NGKsMailcpp with:

- a visible 3-pane main window (folders, message list, reading pane),
- SQLite database initialization in per-user app data,
- append-only JSONL audit logging with SHA-256 hash chaining,
- auditable build/run proof logs under `artifacts/_proof`.

Non-goals in Phase 0: account setup, IMAP/SMTP networking, MIME parsing, background sync.
