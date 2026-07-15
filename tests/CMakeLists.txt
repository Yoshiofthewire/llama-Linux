# One executable per QtTest class, registered with ctest. Task 7 does a
# final consistency pass once Tasks 4-6 add their own tests here.

function(llama_add_test test_name)
  add_executable(${test_name} ${ARGN})
  target_link_libraries(${test_name} PRIVATE llamacore Qt6::Test)
  add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

llama_add_test(EmailTest core/models/EmailTest.cpp)
llama_add_test(ContactTest core/models/ContactTest.cpp)
llama_add_test(StandardFolderTest core/models/StandardFolderTest.cpp)
llama_add_test(KeywordSettingsTest core/models/KeywordSettingsTest.cpp)
llama_add_test(MfaChallengeTest core/models/MfaChallengeTest.cpp)
llama_add_test(PushNotificationTest core/models/PushNotificationTest.cpp)

llama_add_test(DatabaseTest core/db/DatabaseTest.cpp)
llama_add_test(EmailDaoTest core/db/EmailDaoTest.cpp)
llama_add_test(ContactDaoTest core/db/ContactDaoTest.cpp)
llama_add_test(FolderDaoTest core/db/FolderDaoTest.cpp)
llama_add_test(PushDaoTest core/db/PushDaoTest.cpp)
llama_add_test(PendingContactChangeDaoTest core/db/PendingContactChangeDaoTest.cpp)
llama_add_test(NativeContactLinkDaoTest core/db/NativeContactLinkDaoTest.cpp)

llama_add_test(SettingsStoreTest core/stores/SettingsStoreTest.cpp)
llama_add_test(CursorStoreTest core/stores/CursorStoreTest.cpp)
llama_add_test(SecureStoreFileTest core/stores/SecureStoreFileTest.cpp)

llama_add_test(PairingStoreTest core/domain/PairingStoreTest.cpp)
llama_add_test(DeviceRegistrationServiceTest core/domain/DeviceRegistrationServiceTest.cpp)
llama_add_test(MailRepositoryTest core/domain/MailRepositoryTest.cpp)
llama_add_test(KeywordRepositoryTest core/domain/KeywordRepositoryTest.cpp)
llama_add_test(ContactSyncReconciliationTest core/domain/ContactSyncReconciliationTest.cpp)
llama_add_test(NativeContactReconciliationTest core/domain/NativeContactReconciliationTest.cpp)
llama_add_test(ContactSyncRepositoryTest core/domain/ContactSyncRepositoryTest.cpp)
llama_add_test(PushRepositoryTest core/domain/PushRepositoryTest.cpp)
llama_add_test(TransportStateMachineTest core/domain/TransportStateMachineTest.cpp)

llama_add_test(NetworkErrorTest core/net/NetworkErrorTest.cpp)
llama_add_test(HttpClientTest core/net/HttpClientTest.cpp)
llama_add_test(NativeRegistrationClientTest core/net/NativeRegistrationClientTest.cpp)
llama_add_test(PushNotificationClientTest core/net/PushNotificationClientTest.cpp)
llama_add_test(MfaResponseClientTest core/net/MfaResponseClientTest.cpp)
llama_add_test(ContactSyncClientTest core/net/ContactSyncClientTest.cpp)
llama_add_test(RelayMailSourceTest core/net/RelayMailSourceTest.cpp)
llama_add_test(NtfySubscriberTest core/net/NtfySubscriberTest.cpp)

llama_add_test(VCardContactTest core/vcard/VCardContactTest.cpp)

llama_add_test(ThemePaletteTest core/theme/ThemePaletteTest.cpp)
llama_add_test(AppThemeTest core/theme/AppThemeTest.cpp)
llama_add_test(ThemeManagerTest core/theme/ThemeManagerTest.cpp)

# SecureStoreKeychain needs QtKeychain + app/ headers, unlike the plain
# llama_add_test() targets above, so it's its own bespoke add_executable
# block rather than going through the helper.
find_package(Qt6Keychain REQUIRED)
add_executable(SecureStoreKeychainTest
  app/platform/SecureStoreKeychainTest.cpp
  ${CMAKE_SOURCE_DIR}/app/platform/SecureStoreKeychain.cpp
)
target_include_directories(SecureStoreKeychainTest PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(SecureStoreKeychainTest PRIVATE
  llamacore
  Qt6Keychain::Qt6Keychain
  Qt6::Test
)
add_test(NAME SecureStoreKeychainTest COMMAND SecureStoreKeychainTest)

# EmailListModel/MailController (Task 32) live in app/mail/, not llamacore
# (see core/ boundary rule), so -- like SecureStoreKeychainTest above -- they
# need their own bespoke add_executable block instead of llama_add_test():
# app/ headers plus the app/mail/*.cpp sources compiled directly into the
# test binary. Neither needs Qt6Keychain, so no extra find_package/
# target_link_libraries beyond llamacore/Qt6::Test.
add_executable(EmailListModelTest
  app/mail/EmailListModelTest.cpp
  ${CMAKE_SOURCE_DIR}/app/mail/EmailListModel.cpp
)
target_include_directories(EmailListModelTest PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(EmailListModelTest PRIVATE llamacore Qt6::Test)
add_test(NAME EmailListModelTest COMMAND EmailListModelTest)

find_package(KF6I18n REQUIRED)
add_executable(MailControllerTest
  app/mail/MailControllerTest.cpp
  ${CMAKE_SOURCE_DIR}/app/mail/EmailListModel.cpp
  ${CMAKE_SOURCE_DIR}/app/mail/MailController.cpp
)
target_include_directories(MailControllerTest PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(MailControllerTest PRIVATE llamacore Qt6::Test KF6::I18n)
add_test(NAME MailControllerTest COMMAND MailControllerTest)

# ContactListModel/ContactsController (Task 33) live in app/contacts/, not
# llamacore -- same bespoke add_executable pattern as EmailListModelTest/
# MailControllerTest above.
add_executable(ContactListModelTest
  app/contacts/ContactListModelTest.cpp
  ${CMAKE_SOURCE_DIR}/app/contacts/ContactListModel.cpp
)
target_include_directories(ContactListModelTest PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(ContactListModelTest PRIVATE llamacore Qt6::Test)
add_test(NAME ContactListModelTest COMMAND ContactListModelTest)

add_executable(ContactsControllerTest
  app/contacts/ContactsControllerTest.cpp
  ${CMAKE_SOURCE_DIR}/app/contacts/ContactListModel.cpp
  ${CMAKE_SOURCE_DIR}/app/contacts/ContactsController.cpp
)
target_include_directories(ContactsControllerTest PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(ContactsControllerTest PRIVATE llamacore Qt6::Test KF6::I18n)
add_test(NAME ContactsControllerTest COMMAND ContactsControllerTest)

# PairingController/MfaController (Task 34) live in app/pairing/, not
# llamacore -- same bespoke add_executable pattern as MailControllerTest/
# ContactsControllerTest above.
add_executable(PairingControllerTest
  app/pairing/PairingControllerTest.cpp
  ${CMAKE_SOURCE_DIR}/app/pairing/PairingController.cpp
)
target_include_directories(PairingControllerTest PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(PairingControllerTest PRIVATE llamacore Qt6::Test KF6::I18n)
add_test(NAME PairingControllerTest COMMAND PairingControllerTest)

add_executable(MfaControllerTest
  app/pairing/MfaControllerTest.cpp
  ${CMAKE_SOURCE_DIR}/app/pairing/MfaController.cpp
)
target_include_directories(MfaControllerTest PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(MfaControllerTest PRIVATE llamacore Qt6::Test KF6::I18n)
add_test(NAME MfaControllerTest COMMAND MfaControllerTest)

# PushPayloadParser (Task 40) lives in app/push/, not llamacore -- same
# bespoke add_executable pattern as EmailListModelTest/MailControllerTest
# above. Pure C++ over QJsonDocument (already part of Qt6::Core, transitively
# available via llamacore), no extra find_package needed.
#
# NotificationDispatcher's notify() itself has no test here deliberately:
# like UnifiedPushConnector (Task 10, same directory), it's a thin wrapper
# around a concrete, un-mockable system integration point (KNotification's
# real D-Bus round-trip to whatever org.freedesktop.Notifications service is
# reachable, plus the notification server's own ActionInvoked callback for
# the "View" action) -- there's no fake/injectable seam to test through
# without building a mock notification server, which is out of scope. See
# task-40-report.md. Its pure title/text fallback-selection logic
# (pickTitle/pickText) *is* covered, directly, by NotificationDispatcherTest
# below.
add_executable(PushPayloadParserTest
  app/push/PushPayloadParserTest.cpp
  ${CMAKE_SOURCE_DIR}/app/push/PushPayloadParser.cpp
)
target_include_directories(PushPayloadParserTest PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(PushPayloadParserTest PRIVATE llamacore Qt6::Test)
add_test(NAME PushPayloadParserTest COMMAND PushPayloadParserTest)

# NotificationDispatcherTest (review follow-up to Task 40): covers only the
# pure pickTitle()/pickText() static helpers, not notify() -- see the
# PushPayloadParserTest comment above for why notify() itself stays
# untested. Needs KF6::Notifications (same find_package as app/CMakeLists.txt)
# because NotificationDispatcher.cpp still #includes <KNotification> for
# notify(), even though this test never calls it -- same bespoke
# add_executable pattern as PushPayloadParserTest/MailControllerTest above,
# not llama_add_test(), since it needs app/ include dirs plus this extra
# KF6 package.
find_package(KF6Notifications REQUIRED)
add_executable(NotificationDispatcherTest
  app/push/NotificationDispatcherTest.cpp
  ${CMAKE_SOURCE_DIR}/app/push/NotificationDispatcher.cpp
)
target_include_directories(NotificationDispatcherTest PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(NotificationDispatcherTest PRIVATE llamacore KF6::Notifications Qt6::Test KF6::I18n)
add_test(NAME NotificationDispatcherTest COMMAND NotificationDispatcherTest)

# NtfyTopicProvisioner (Task 43 review-finding fix): generation/persistence
# logic for the EmbeddedSubscriber tier's ntfy topic, extracted to app/push/
# per that task's brief. Tests against a real SecureStoreFile (core/stores/,
# already part of llamacore), same bespoke add_executable pattern as
# PushPayloadParserTest above -- no extra find_package needed, plain QtCore
# (QRandomGenerator).
add_executable(NtfyTopicProvisionerTest
  app/push/NtfyTopicProvisionerTest.cpp
  ${CMAKE_SOURCE_DIR}/app/push/NtfyTopicProvisioner.cpp
)
target_include_directories(NtfyTopicProvisionerTest PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(NtfyTopicProvisionerTest PRIVATE llamacore Qt6::Test)
add_test(NAME NtfyTopicProvisionerTest COMMAND NtfyTopicProvisionerTest)
