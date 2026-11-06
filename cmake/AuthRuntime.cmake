add_library(batbox_locking src/locking/FileOps.cpp src/locking/ProperLock.cpp
  src/locking/Heartbeat.cpp)
target_link_libraries(batbox_locking PUBLIC batbox_core batbox_event_loop)
target_compile_options(batbox_locking PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_auth PRIVATE
  src/auth/storage/Fallback.cpp
  src/auth/storage/KeychainHelpers.cpp
  src/auth/storage/MacKeychain.cpp
  src/auth/storage/IndigenousPlaintext.cpp
  src/auth/storage/IndigenousSecurity.cpp
  src/auth/storage/Plaintext.cpp
)
target_link_libraries(batbox_auth PUBLIC batbox_locking batbox_process CURL::libcurl OpenSSL::Crypto)
target_sources(batbox_auth PRIVATE src/auth/oauth_client/FormUrl.cpp)
target_link_libraries(batbox_auth PUBLIC batbox_url)
target_sources(batbox_auth PRIVATE src/auth/oauth_listener/IndigenousServerUv.cpp)
