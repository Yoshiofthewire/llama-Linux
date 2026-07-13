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

llama_add_test(SettingsStoreTest core/stores/SettingsStoreTest.cpp)
llama_add_test(CursorStoreTest core/stores/CursorStoreTest.cpp)
llama_add_test(SecureStoreFileTest core/stores/SecureStoreFileTest.cpp)

llama_add_test(NetworkErrorTest core/net/NetworkErrorTest.cpp)
llama_add_test(HttpClientTest core/net/HttpClientTest.cpp)
llama_add_test(NativeRegistrationClientTest core/net/NativeRegistrationClientTest.cpp)
llama_add_test(PushNotificationClientTest core/net/PushNotificationClientTest.cpp)

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
