add_library(batbox_locking src/locking/FileOps.cpp src/locking/ProperLock.cpp
  src/locking/Heartbeat.cpp src/locking/Bindings.cpp)
target_link_libraries(batbox_locking PUBLIC batbox_core batbox_event_loop)
target_compile_options(batbox_locking PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_auth PRIVATE
  src/auth/refresh/Check.cpp
  src/auth/refresh/Invalidate.cpp
  src/auth/refresh/LockBinding.cpp
  src/auth/refresh/IndigenousHttp.cpp
  src/auth/refresh/IndigenousStat.cpp
  src/auth/refresh/Profile.cpp
  src/auth/refresh/ProfileHttp.cpp
  src/auth/refresh/Refresh.cpp
  src/auth/recovery/Handle401.cpp
  src/auth/save/ApiKeySave.cpp
  src/auth/save/IndigenousApiKeySave.cpp
  src/auth/save/SaveTokens.cpp
  src/auth/storage/AuthBinding.cpp
  src/auth/storage/Fallback.cpp
  src/auth/storage/KeychainHelpers.cpp
  src/auth/storage/KeychainLock.cpp
  src/auth/storage/KeychainPrefetch.cpp
  src/auth/storage/MacKeychain.cpp
  src/auth/storage/IndigenousPlaintext.cpp
  src/auth/storage/IndigenousPrefetch.cpp
  src/auth/storage/IndigenousSecurity.cpp
  src/auth/storage/Plaintext.cpp
)
target_link_libraries(batbox_auth PUBLIC batbox_locking batbox_process CURL::libcurl OpenSSL::Crypto)
target_sources(batbox_core PRIVATE src/gadgets/SchemaStash.cpp)
add_library(batbox_betas src/betas/Models.cpp src/betas/Filtering.cpp
  src/betas/HeaderSelection.cpp src/betas/Stash.cpp src/betas/WireValues.cpp)
target_link_libraries(batbox_betas PUBLIC batbox_core ICU::uc)
target_compile_options(batbox_betas PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_auth PRIVATE src/auth/oauth_client/AuthUrl.cpp
  src/auth/oauth_client/FormUrl.cpp src/auth/oauth_client/TokenExchange.cpp
  src/auth/oauth_client/IndigenousBinding.cpp src/auth/oauth_client/AccountActions.cpp
  src/auth/oauth_client/IndigenousAccountBinding.cpp src/auth/oauth_client/IndigenousAccountHttp.cpp)
target_link_libraries(batbox_auth PUBLIC batbox_url)
target_sources(batbox_auth PRIVATE src/auth/oauth_client/ProfileAccount.cpp
  src/auth/oauth_client/ProfileScope.cpp src/auth/oauth_client/IndigenousProfileBinding.cpp)

add_library(batbox_oauth_config src/client_factory/oauth_config/OAuthConfig.cpp
  src/client_factory/oauth_config/WireValues.cpp)
target_link_libraries(batbox_oauth_config PUBLIC batbox_core)
target_compile_options(batbox_oauth_config PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(batbox_auth PUBLIC batbox_oauth_config)
target_sources(batbox_auth PRIVATE src/auth/oauth_listener/AuthCodeListener.cpp
  src/auth/oauth_listener/IndigenousServerUv.cpp)
