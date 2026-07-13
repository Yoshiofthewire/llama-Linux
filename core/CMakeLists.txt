add_library(llamacore STATIC
  models/StandardFolder.cpp
  models/Email.h
  models/Contact.h
  models/StandardFolder.h
  models/KeywordSettings.h
  models/MfaChallenge.h
  models/PushNotification.h
  db/Database.cpp
  db/Database.h
  db/EmailDao.cpp
  db/EmailDao.h
  db/ContactDao.cpp
  db/ContactDao.h
  db/FolderDao.cpp
  db/FolderDao.h
  db/PushDao.cpp
  db/PushDao.h
  db/SqlUtil.h
  stores/SettingsStore.cpp
  stores/SettingsStore.h
  stores/CursorStore.cpp
  stores/CursorStore.h
  stores/SecureStore.h
  stores/SecureStoreFile.cpp
  stores/SecureStoreFile.h
)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/db/migrations/001_initial.sql LLAMA_MIGRATION_001_SQL)
configure_file(db/MigrationSql.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/MigrationSql.h @ONLY)

target_include_directories(llamacore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(llamacore PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)

target_link_libraries(llamacore PUBLIC
  ${QT_TARGET_PREFIX}::Core
  ${QT_TARGET_PREFIX}::Network
  ${QT_TARGET_PREFIX}::Sql
)
