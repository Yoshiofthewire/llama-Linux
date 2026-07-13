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
