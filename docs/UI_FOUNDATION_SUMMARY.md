<!-- markdownlint-disable MD013 MD041 -->

# UI Foundation Summary

## Delivered

- Replaced minimal shell with a structured 3-pane `QMainWindow` foundation.
- Added account/folder/message model architecture with service adapter abstraction.
- Added toolbar controls and audit placeholder action logging.
- Added splitter/geometry persistence via `QSettings`.
- Added palette-compatible baseline styling and unread emphasis.

## File Highlights

- `src/ui/MainWindow.h`
- `src/ui/MainWindow.cpp`
- `src/ui/models/AccountTreeModel.hpp`
- `src/ui/models/AccountTreeModel.cpp`
- `src/ui/models/FolderTreeModel.hpp`
- `src/ui/models/FolderTreeModel.cpp`
- `src/ui/models/MessageListModel.hpp`
- `src/ui/models/MessageListModel.cpp`
- `src/ui/models/MailUiService.hpp`

## Deferred / TODO

- Replace stub service with threaded production data service.
- Implement real compose/delete/mark-read behavior.
- Add message pagination and virtualized loading.
- Add richer preview sanitization and MIME handling.
