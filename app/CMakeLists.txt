add_executable(llamamail
  main.cpp
  qml/qml.qrc
)

target_include_directories(llamamail PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(llamamail PRIVATE
  llamacore
  Qt6::Quick
  Qt6::Qml
  Qt6::QuickControls2
  KF6::Kirigami
)

# EmailDetail.qml's body WebEngineView (Task 35) -- Qt6::WebEngineQuick
# provides the "QtWebEngine" QML module (WebEngineView/
# WebEngineNavigationRequest) at runtime; confirmed installed
# (qt6-webengine 6.11.1) this session. No app/*.cpp source touches this
# API directly -- it's QML-only, so no target_sources entry is needed here,
# just the find_package/target_link_libraries to make the plugin available
# at runtime and satisfy qmllint/IDE tooling.
find_package(Qt6 REQUIRED COMPONENTS WebEngineQuick)
target_link_libraries(llamamail PRIVATE Qt6::WebEngineQuick)

# ThemeController wraps core::ThemeManager as a QML singleton (Q_PROPERTY/
# QColor/qmlRegisterSingletonInstance), so it belongs in the app executable,
# never in llamacore -- Qt6::Gui (QColor) and Qt6::Qml (qmlRegisterSingleton*)
# are already available transitively via Qt6::Quick above, no new
# find_package/target_link_libraries needed. fonts.qrc bundles the real TTF
# files ThemeController.fontUi()/fontMono() advertise the family names of;
# same AUTORCC mechanism already used for qml/qml.qrc.
target_sources(llamamail PRIVATE
  theme/ThemeController.cpp
  theme/ThemeController.h
  resources/fonts.qrc
)

# MailController/EmailListModel (Task 32) wrap core/domain's MailRepository/
# KeywordRepository/PairingStore and core/net's RelayMailSource as a QML
# singleton ("MailApp") -- QObject/QAbstractListModel/QVariant, so app/ per
# the core/ boundary rule, same as ThemeController above. No extra
# find_package needed: QMimeDatabase/QStandardPaths/QFile are already part of
# Qt6::Core, transitively available via Qt6::Quick.
target_sources(llamamail PRIVATE
  mail/EmailListModel.cpp
  mail/EmailListModel.h
  mail/MailController.cpp
  mail/MailController.h
)

# ContactsController/ContactListModel (Task 33) wrap core/domain's
# ContactSyncRepository as a QML singleton ("ContactsApp") -- QObject/
# QAbstractListModel/QVariant, so app/ per the core/ boundary rule, same as
# MailController/EmailListModel above. No extra find_package needed.
target_sources(llamamail PRIVATE
  contacts/ContactListModel.cpp
  contacts/ContactListModel.h
  contacts/ContactsController.cpp
  contacts/ContactsController.h
)

# PairingController/MfaController (Task 34) wrap core/domain's
# DeviceRegistrationService/PairingStore and core/net's MfaResponseClient as
# QML singletons ("Pairing"/"Mfa") -- QObject/QUrl parsing, so app/ per the
# core/ boundary rule, same as MailController/ContactsController above. No
# extra find_package needed: QUrlQuery is already part of Qt6::Core,
# transitively available via Qt6::Quick.
target_sources(llamamail PRIVATE
  pairing/MfaController.cpp
  pairing/MfaController.h
  pairing/PairingController.cpp
  pairing/PairingController.h
)

# SecureStoreKeychain talks to the Secret Service over QtDBus, so it belongs
# in the app executable, never in llamacore.
find_package(Qt6Keychain REQUIRED)
target_sources(llamamail PRIVATE
  platform/SecureStoreKeychain.cpp
  platform/SecureStoreKeychain.h
)
target_link_libraries(llamamail PRIVATE Qt6Keychain::Qt6Keychain)

# UnifiedPushConnector talks to the local KUnifiedPush distributor over
# D-Bus, so it belongs in the app executable, never in llamacore. The
# KUnifiedPushConfig.cmake package exports a plain "KUnifiedPush" target
# (no namespace), confirmed against the installed .cmake files. Qt6::DBus is
# needed directly (not just transitively via KUnifiedPush) because
# UnifiedPushConnector registers its own well-known D-Bus service name for
# the distributor's callback delivery -- see UnifiedPushConnector.cpp.
find_package(KUnifiedPush REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS DBus)
target_sources(llamamail PRIVATE
  push/UnifiedPushConnector.cpp
  push/UnifiedPushConnector.h
)
target_link_libraries(llamamail PRIVATE KUnifiedPush Qt6::DBus)

# PushPayloadParser (pure JSON-bytes -> PushNotification parsing, no QObject)
# and NotificationDispatcher (wraps KNotification) (Task 40) live in
# app/push/, alongside UnifiedPushConnector -- KNotifications is a KF6 system
# library, never linked into llamacore, same core/ boundary rule. The
# KF6NotificationsConfig.cmake package exports a "KF6::Notifications" target
# (confirmed via `pacman -Ql knotifications | grep cmake` and its Targets
# .cmake file this session), same naming pattern as KF6::DBusAddons below.
find_package(KF6Notifications REQUIRED)
target_sources(llamamail PRIVATE
  push/NotificationDispatcher.cpp
  push/NotificationDispatcher.h
  push/PushPayloadParser.cpp
  push/PushPayloadParser.h
)
target_link_libraries(llamamail PRIVATE KF6::Notifications)

# NtfyTopicProvisioner (Task 43 review-finding fix): generates/persists the
# EmbeddedSubscriber tier's ntfy topic via SecureStore -- plain QtCore
# (QRandomGenerator), no extra find_package needed. app/ per the core/
# boundary rule, same as NotificationDispatcher/PushPayloadParser above.
target_sources(llamamail PRIVATE
  push/NtfyTopicProvisioner.cpp
  push/NtfyTopicProvisioner.h
)

# KDBusService (KF6DBusAddons) wraps app startup in main.cpp to claim the
# com.urlxl.LlamaMail well-known bus name in Unique mode and emit
# activateRequested on duplicate-instance launches -- confirmed installed via
# `pacman -Qs kdbusaddons`, providing the standard KDE convenience wrapper
# over plain QDBusConnection registration for Task 11's D-Bus activation
# proof. See main.cpp for why org.freedesktop.Application/Activate isn't used.
find_package(KF6DBusAddons REQUIRED)
target_link_libraries(llamamail PRIVATE KF6::DBusAddons)

include(GNUInstallDirs)
install(TARGETS llamamail RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
