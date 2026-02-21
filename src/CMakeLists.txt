set(NGKSMAILCPP_SOURCES
  app/main.cpp
  app/App.cpp
  ui/MainWindow.cpp
  core/logging/AuditLog.cpp
  core/storage/Db.cpp
  platform/common/Paths.cpp
)

add_executable(NGKsMailcpp ${NGKSMAILCPP_SOURCES})

target_include_directories(NGKsMailcpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(NGKsMailcpp
  PRIVATE
    Qt6::Core
    Qt6::Widgets
    SQLite::SQLite3
)
