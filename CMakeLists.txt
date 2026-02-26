cmake_minimum_required(VERSION 3.21)
project(NGKsMailcpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Sql Network Gui)

set(NGKSMAIL_CORE0_SOURCES
	src/core/config/SettingsStore.cpp
	src/core/auth/OAuthStore.cpp
	src/core/logging/AuditLog.cpp
	src/core/oauth/OAuthBroker.cpp
	src/providers/core/ProviderRegistry.cpp
	src/providers/gmail/GmailProfile.cpp
	src/providers/gmail/GmailAuthDriver.cpp
	src/providers/generic_imap/GenericImapProfile.cpp
	src/providers/generic_imap/GenericImapAuthDriver.cpp
	src/providers/icloud/ICloudProfile.cpp
	src/providers/icloud/ICloudAuthDriver.cpp
	src/providers/ms_graph/MsGraphProfile.cpp
	src/providers/ms_graph/MsGraphAuthDriver.cpp
	src/providers/yahoo_app_password/YahooAppPasswordProfile.cpp
	src/providers/yahoo_app_password/YahooAppPasswordAuthDriver.cpp
	src/providers/yahoo/YahooProfile.cpp
	src/providers/yahoo/YahooAuthDriver.cpp
	src/resolve/ProviderDiscovery.cpp
	src/db/BasicCredentialStore.cpp
	src/db/TokenStore.cpp
	src/core/storage/Db.cpp
	src/core/storage/Schema.cpp
	src/core/mail/providers/imap/ImapClient.cpp
	src/core/mail/providers/imap/ImapProvider.cpp
	src/core/mail/providers/imap/FolderMirrorService.cpp
	src/platform/common/Paths.cpp
)

add_library(ngksmail_core0 STATIC ${NGKSMAIL_CORE0_SOURCES})
target_include_directories(ngksmail_core0 PUBLIC src)
target_link_libraries(ngksmail_core0 PUBLIC Qt6::Core Qt6::Sql Qt6::Network Qt6::Gui)

set(NGKSMAIL_UI0_SOURCES
	src/ui/MainWindow.cpp
	src/ui/models/AccountTreeModel.cpp
	src/ui/models/FolderTreeModel.cpp
	src/ui/models/MessageListModel.cpp
	src/ui/models/ThreadModel.cpp
	src/ui/shell/ComposeWindow.cpp
	src/ui/shell/MessageList.cpp
	src/ui/shell/NavigationPane.cpp
	src/ui/shell/ReadingPane.cpp
)

add_library(ngksmail_ui0 STATIC ${NGKSMAIL_UI0_SOURCES})
target_include_directories(ngksmail_ui0 PUBLIC src)
target_link_libraries(ngksmail_ui0 PUBLIC Qt6::Core Qt6::Widgets Qt6::Sql)

add_executable(NGKsMailcpp
	src/app/main.cpp
	src/app/App.cpp
)

target_include_directories(NGKsMailcpp PRIVATE src)
target_link_libraries(NGKsMailcpp PRIVATE ngksmail_core0 ngksmail_ui0 Qt6::Core Qt6::Widgets Qt6::Sql Qt6::Network)
