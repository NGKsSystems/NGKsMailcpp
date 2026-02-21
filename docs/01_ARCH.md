<!-- markdownlint-disable MD013 MD022 MD032 -->
# 01 Arch

Runtime flow:

1. `main.cpp` starts `QApplication` and calls `App::Run`.
2. `App` ensures paths/directories, opens SQLite DB, ensures schema, writes `APP_START` audit event.
3. `MainWindow` shows a 3-pane splitter with placeholder labels.

Phase-0 modules:

- `src/app`: startup orchestration.
- `src/ui`: Qt Widgets shell.
- `src/core/storage`: SQLite open + schema creation.
- `src/core/logging`: append-only JSONL audit with hash chain.
- `src/platform/common`: per-user app data + repo artifacts paths.
