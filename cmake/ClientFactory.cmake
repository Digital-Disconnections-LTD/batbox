add_library(batbox_client_factory src/client_factory/Factory.cpp
  src/client_factory/Direct.cpp src/client_factory/Regions.cpp
  src/client_factory/proxy/ProxyOptions.cpp)
target_link_libraries(batbox_client_factory PUBLIC batbox_transport batbox_event_loop batbox_core)
target_compile_options(batbox_client_factory PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_client_factory PRIVATE src/client_factory/tls/Mtls.cpp
  src/client_factory/tls/CaCerts.cpp src/client_factory/tls/FetchOptions.cpp
  src/client_factory/tls/IndigenousCerts.cpp src/client_factory/tls/IndigenousRoots.cpp
  src/client_factory/AuthEnabled.cpp
  src/client_factory/IndigenousAuthBinding.cpp src/client_factory/Subscriber.cpp
  src/client_factory/UserBat.cpp)
target_link_libraries(batbox_client_factory PUBLIC batbox_auth)
target_link_libraries(batbox_client_factory PUBLIC batbox_oauth_config)
if(WIN32)
  target_link_libraries(batbox_client_factory PUBLIC crypt32)
endif()
