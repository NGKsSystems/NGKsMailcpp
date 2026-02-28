<!-- markdownlint-disable MD013 MD024 MD041 -->

# UI Foundation Details

## Scope

This phase replaces the minimal shell with a production-grade 3-pane mail aggregator foundation while keeping provider/auth/audit/database backend logic unchanged.

## Implemented Architecture

- Main window is now a true 3-pane layout:
  - Left: account tree + folder tree in vertical splitter.
  - Center: message list table.
  - Right: HTML-capable preview (`QTextBrowser`) with lazy load on row selection.
- Horizontal splitter controls left/center/right panes with minimum widths and non-collapsible behavior.
- Geometry and splitter state persist through `QSettings`.

## Model Layer

Created/introduced model-first UI data boundaries:

- `src/ui/models/AccountTreeModel.hpp` + `.cpp`
- `src/ui/models/FolderTreeModel.hpp` + `.cpp`
- `src/ui/models/MessageListModel.hpp` + `.cpp`
- `src/ui/models/MailUiService.hpp` (service abstraction + stub adapter)

Design choices:

- Views do not query DB directly.
- Models load through a service abstraction (`IMailUiService`).
- Temporary `StubMailUiService` provides placeholder data while preserving the long-term architecture.

## Async Readiness

Prepared for non-blocking future loading by isolating data access behind the service boundary and using slot-based refresh/update flows (`OnRefresh`, account/folder/message change handlers). Thread workers can later be attached without redesigning view/model responsibilities.

## Toolbar and UX Controls

Added top toolbar actions with placeholder behavior:

- Refresh
- Compose
- Delete
- Mark Read
- Settings

Current behavior logs action intents to audit (`UI_ACTION`) via existing logging utility, preserving traceability without implementing action business logic yet.

## Styling Foundation

Base style sheet now defines:

- neutral palette-driven surfaces,
- hover highlighting,
- selected row highlight,
- alternate row backgrounds,
- unread emphasis via bold font in `MessageListModel`.

Palette-based colors are used to remain dark-mode compatible.

## Notes

- Legacy headers (`AccountTreeModel.h`, `FolderTreeModel.h`, `MessageListModel.h`) are retained as compatibility shims that include the new `.hpp` interfaces.
- This phase does not modify provider drivers, OAuth logic, schema internals, FolderMirrorService, or audit routing mechanics.
