# One executable per QtTest class, registered with ctest. Task 7 does a
# final consistency pass once Tasks 4-6 add their own tests here.

function(llama_add_test test_name)
  add_executable(${test_name} ${ARGN})
  target_link_libraries(${test_name} PRIVATE llamacore ${QT_TARGET_PREFIX}::Test)
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

# SecureStoreKeychain needs QtKeychain + app/ headers, unlike the plain
# llama_add_test() targets above, and only builds under Qt6 (only
# qtkeychain-qt6 is installed on this machine).
if(LLAMA_QT6)
  find_package(Qt6Keychain REQUIRED)
  add_executable(SecureStoreKeychainTest
    app/platform/SecureStoreKeychainTest.cpp
    ${CMAKE_SOURCE_DIR}/app/platform/SecureStoreKeychain.cpp
  )
  target_include_directories(SecureStoreKeychainTest PRIVATE ${CMAKE_SOURCE_DIR}/app)
  target_link_libraries(SecureStoreKeychainTest PRIVATE
    llamacore
    Qt6Keychain::Qt6Keychain
    ${QT_TARGET_PREFIX}::Test
  )
  add_test(NAME SecureStoreKeychainTest COMMAND SecureStoreKeychainTest)
endif()
