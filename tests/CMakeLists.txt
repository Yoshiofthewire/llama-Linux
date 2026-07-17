# One executable per QtTest class, registered with ctest. Task 7 does a
# final consistency pass once Tasks 4-6 add their own tests here.

function(llama_add_test test_name)
  add_executable(${test_name} ${ARGN})
  target_link_libraries(${test_name} PRIVATE llamacore Qt6::Test)
  add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

# Like llama_add_test() above, but for tests that live in app/ rather than
# llamacore: needs the app/ include directory (so headers under app/ resolve
# the same way they do for the llamamail target itself) and an explicit
# SOURCES list (the extra app/*.cpp files under test, compiled directly into
# the test binary rather than linked from a library), plus an optional LIBS
# list for tests that need packages beyond llamacore/Qt6::Test (e.g.
# KF6::I18n, Qt6Keychain::Qt6Keychain). Wraps the add_executable +
# target_include_directories(PRIVATE app) + target_link_libraries + add_test
# pattern repeated below for each app/ test target.
function(llama_add_app_test test_name)
  cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
  add_executable(${test_name} ${ARG_SOURCES})
  target_include_directories(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/app)
  target_link_libraries(${test_name} PRIVATE llamacore Qt6::Test ${ARG_LIBS})
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
llama_add_test(GroupDaoTest core/db/GroupDaoTest.cpp)
llama_add_test(FolderDaoTest core/db/FolderDaoTest.cpp)
llama_add_test(PushDaoTest core/db/PushDaoTest.cpp)
llama_add_test(PendingContactChangeDaoTest core/db/PendingContactChangeDaoTest.cpp)
llama_add_test(NativeContactLinkDaoTest core/db/NativeContactLinkDaoTest.cpp)

llama_add_test(SettingsStoreTest core/stores/SettingsStoreTest.cpp)
llama_add_test(CursorStoreTest core/stores/CursorStoreTest.cpp)
llama_add_test(SecureStoreFileTest core/stores/SecureStoreFileTest.cpp)
llama_add_test(ContactPhotoCacheTest core/stores/ContactPhotoCacheTest.cpp)

llama_add_test(PairingStoreTest core/domain/PairingStoreTest.cpp)
llama_add_test(DeviceRegistrationServiceTest core/domain/DeviceRegistrationServiceTest.cpp)
llama_add_test(MailRepositoryTest core/domain/MailRepositoryTest.cpp)
llama_add_test(KeywordRepositoryTest core/domain/KeywordRepositoryTest.cpp)
llama_add_test(ContactSyncReconciliationTest core/domain/ContactSyncReconciliationTest.cpp)
llama_add_test(NativeContactReconciliationTest core/domain/NativeContactReconciliationTest.cpp)
llama_add_test(ContactSyncRepositoryTest core/domain/ContactSyncRepositoryTest.cpp)
llama_add_test(GroupsRepositoryTest core/domain/GroupsRepositoryTest.cpp)
llama_add_test(ContactPhotoRepositoryTest core/domain/ContactPhotoRepositoryTest.cpp)
llama_add_test(PushRepositoryTest core/domain/PushRepositoryTest.cpp)
llama_add_test(TransportStateMachineTest core/domain/TransportStateMachineTest.cpp)
llama_add_test(PgpQrRepositoryTest core/domain/PgpQrRepositoryTest.cpp)

llama_add_test(NetworkErrorTest core/net/NetworkErrorTest.cpp)
llama_add_test(HttpClientTest core/net/HttpClientTest.cpp)
llama_add_test(NativeRegistrationClientTest core/net/NativeRegistrationClientTest.cpp)
llama_add_test(PushNotificationClientTest core/net/PushNotificationClientTest.cpp)
llama_add_test(MfaResponseClientTest core/net/MfaResponseClientTest.cpp)
llama_add_test(ContactSyncClientTest core/net/ContactSyncClientTest.cpp)
llama_add_test(GroupsClientTest core/net/GroupsClientTest.cpp)
llama_add_test(ContactPhotoClientTest core/net/ContactPhotoClientTest.cpp)
llama_add_test(PgpQrClientTest core/net/PgpQrClientTest.cpp)
llama_add_test(RelayMailSourceTest core/net/RelayMailSourceTest.cpp)
llama_add_test(NtfySubscriberTest core/net/NtfySubscriberTest.cpp)

llama_add_test(VCardContactTest core/vcard/VCardContactTest.cpp)

llama_add_test(ThemePaletteTest core/theme/ThemePaletteTest.cpp)
llama_add_test(AppThemeTest core/theme/AppThemeTest.cpp)
llama_add_test(ThemeManagerTest core/theme/ThemeManagerTest.cpp)

# SecureStoreKeychain needs QtKeychain + app/ headers, so its LIBS list adds
# Qt6Keychain::Qt6Keychain on top of llama_add_app_test()'s usual
# llamacore/Qt6::Test.
find_package(Qt6Keychain REQUIRED)
llama_add_app_test(SecureStoreKeychainTest
  SOURCES
    app/platform/SecureStoreKeychainTest.cpp
    ${CMAKE_SOURCE_DIR}/app/platform/SecureStoreKeychain.cpp
  LIBS Qt6Keychain::Qt6Keychain
)

# EmailListModel/MailController (Task 32) live in app/mail/, not llamacore
# (see core/ boundary rule): app/ headers plus the app/mail/*.cpp sources
# compiled directly into the test binary. EmailListModelTest needs no extra
# LIBS beyond llamacore/Qt6::Test.
llama_add_app_test(EmailListModelTest
  SOURCES
    app/mail/EmailListModelTest.cpp
    ${CMAKE_SOURCE_DIR}/app/mail/EmailListModel.cpp
)

find_package(KF6I18n REQUIRED)
llama_add_app_test(MailControllerTest
  SOURCES
    app/mail/MailControllerTest.cpp
    ${CMAKE_SOURCE_DIR}/app/mail/EmailListModel.cpp
    ${CMAKE_SOURCE_DIR}/app/mail/MailController.cpp
  LIBS KF6::I18n
)

# ContactListModel/ContactsController (Task 33) live in app/contacts/, not
# llamacore -- same app/ test pattern as EmailListModelTest/MailControllerTest
# above.
llama_add_app_test(ContactListModelTest
  SOURCES
    app/contacts/ContactListModelTest.cpp
    ${CMAKE_SOURCE_DIR}/app/contacts/ContactListModel.cpp
)

llama_add_app_test(ContactsControllerTest
  SOURCES
    app/contacts/ContactsControllerTest.cpp
    ${CMAKE_SOURCE_DIR}/app/contacts/ContactListModel.cpp
    ${CMAKE_SOURCE_DIR}/app/contacts/ContactsController.cpp
  LIBS KF6::I18n
)

# PairingController/MfaController (Task 34) live in app/pairing/, not
# llamacore -- same app/ test pattern as MailControllerTest/
# ContactsControllerTest above.
llama_add_app_test(PairingControllerTest
  SOURCES
    app/pairing/PairingControllerTest.cpp
    ${CMAKE_SOURCE_DIR}/app/pairing/PairingController.cpp
  LIBS KF6::I18n
)

llama_add_app_test(MfaControllerTest
  SOURCES
    app/pairing/MfaControllerTest.cpp
    ${CMAKE_SOURCE_DIR}/app/pairing/MfaController.cpp
  LIBS KF6::I18n
)

# PgpQrController (PGP QR key exchange) lives in app/pgp/, not llamacore --
# same app/ test pattern as PairingControllerTest/MfaControllerTest above.
# Needs Qt6::Gui (QImage/QBuffer) and ZXing::ZXing (plain ZXing:: C++ API,
# see app/CMakeLists.txt's PgpQrController comment) in addition to KF6::I18n.
find_package(ZXing CONFIG REQUIRED)
llama_add_app_test(PgpQrControllerTest
  SOURCES
    app/pgp/PgpQrControllerTest.cpp
    ${CMAKE_SOURCE_DIR}/app/pgp/PgpQrController.cpp
  LIBS Qt6::Gui KF6::I18n ZXing::ZXing
)

# PushPayloadParser (Task 40) lives in app/push/, not llamacore -- same
# app/ test pattern as EmailListModelTest/MailControllerTest above. Pure C++
# over QJsonDocument (already part of Qt6::Core, transitively available via
# llamacore), no extra LIBS needed.
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
llama_add_app_test(PushPayloadParserTest
  SOURCES
    app/push/PushPayloadParserTest.cpp
    ${CMAKE_SOURCE_DIR}/app/push/PushPayloadParser.cpp
)

# NotificationDispatcherTest (review follow-up to Task 40): covers only the
# pure pickTitle()/pickText() static helpers, not notify() -- see the
# PushPayloadParserTest comment above for why notify() itself stays
# untested. Needs KF6::Notifications (same find_package as app/CMakeLists.txt)
# because NotificationDispatcher.cpp still #includes <KNotification> for
# notify(), even though this test never calls it.
find_package(KF6Notifications REQUIRED)
llama_add_app_test(NotificationDispatcherTest
  SOURCES
    app/push/NotificationDispatcherTest.cpp
    ${CMAKE_SOURCE_DIR}/app/push/NotificationDispatcher.cpp
  LIBS KF6::Notifications KF6::I18n
)

# NtfyTopicProvisioner (Task 43 review-finding fix): generation/persistence
# logic for the EmbeddedSubscriber tier's ntfy topic, extracted to app/push/
# per that task's brief. Tests against a real SecureStoreFile (core/stores/,
# already part of llamacore), same app/ test pattern as PushPayloadParserTest
# above -- no extra LIBS needed, plain QtCore (QRandomGenerator).
llama_add_app_test(NtfyTopicProvisionerTest
  SOURCES
    app/push/NtfyTopicProvisionerTest.cpp
    ${CMAKE_SOURCE_DIR}/app/push/NtfyTopicProvisioner.cpp
)
