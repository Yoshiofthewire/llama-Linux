add_library(llamacore STATIC
  models/StandardFolder.cpp
  models/Email.h
  models/Contact.h
  models/StandardFolder.h
  models/KeywordSettings.h
  models/MfaChallenge.h
  models/PushNotification.h
  domain/DevicePairing.h
  domain/PairingStore.cpp
  domain/PairingStore.h
  domain/DeviceRegistrationService.cpp
  domain/DeviceRegistrationService.h
  domain/MailRepository.cpp
  domain/MailRepository.h
  domain/KeywordRepository.cpp
  domain/KeywordRepository.h
  domain/ContactSyncReconciliation.cpp
  domain/ContactSyncReconciliation.h
  domain/ContactSyncRepository.cpp
  domain/ContactSyncRepository.h
  domain/PushRepository.cpp
  domain/PushRepository.h
  domain/TransportStateMachine.cpp
  domain/TransportStateMachine.h
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
  db/PendingContactChangeDao.cpp
  db/PendingContactChangeDao.h
  db/SqlUtil.h
  stores/SettingsStore.cpp
  stores/SettingsStore.h
  stores/CursorStore.cpp
  stores/CursorStore.h
  stores/SecureStore.h
  stores/SecureStoreFile.cpp
  stores/SecureStoreFile.h
  net/NetworkError.cpp
  net/NetworkError.h
  net/RelayAuth.h
  net/HttpClient.cpp
  net/HttpClient.h
  net/NativeRegistrationClient.cpp
  net/NativeRegistrationClient.h
  net/PushNotificationClient.cpp
  net/PushNotificationClient.h
  net/MfaResponseClient.cpp
  net/MfaResponseClient.h
  net/ContactSyncClient.cpp
  net/ContactSyncClient.h
  net/RelayMailSource.cpp
  net/RelayMailSource.h
  net/NtfySubscriber.cpp
  net/NtfySubscriber.h
  theme/ThemePalette.cpp
  theme/ThemePalette.h
  theme/AppTheme.cpp
  theme/AppTheme.h
  theme/SemanticColors.h
  theme/Shape.h
)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/db/migrations/001_initial.sql LLAMA_MIGRATION_001_SQL)
configure_file(db/MigrationSql.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/MigrationSql.h @ONLY)

target_include_directories(llamacore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(llamacore PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)

target_link_libraries(llamacore PUBLIC
  Qt6::Core
  Qt6::Network
  Qt6::Sql
)
