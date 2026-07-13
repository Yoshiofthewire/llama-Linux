add_executable(llamamail
  main.cpp
  qml/qml.qrc
)

target_include_directories(llamamail PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(llamamail PRIVATE
  llamacore
  ${QT_TARGET_PREFIX}::Quick
  ${QT_TARGET_PREFIX}::Qml
  ${QT_TARGET_PREFIX}::QuickControls2
  ${KIRIGAMI_TARGET}
)

# SecureStoreKeychain talks to the Secret Service over QtDBus, so it belongs
# in the app executable, never in llamacore. Only Qt6Keychain is installed on
# this machine, so it's Qt6-only for now.
if(LLAMA_QT6)
  find_package(Qt6Keychain REQUIRED)
  target_sources(llamamail PRIVATE
    platform/SecureStoreKeychain.cpp
    platform/SecureStoreKeychain.h
  )
  target_link_libraries(llamamail PRIVATE Qt6Keychain::Qt6Keychain)
endif()
