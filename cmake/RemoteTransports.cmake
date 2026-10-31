find_package(PkgConfig REQUIRED)
pkg_check_modules(BATBOX_WEBSOCKETS REQUIRED IMPORTED_TARGET libwebsockets)

add_library(batbox_remote
  src/cli/remote/CcrEvents.cpp src/cli/remote/CcrFactory.cpp
  src/cli/remote/CcrLifecycle.cpp src/cli/remote/CcrReads.cpp
  src/cli/remote/CcrRequest.cpp src/cli/remote/HybridCore.cpp
  src/cli/remote/HybridPost.cpp src/cli/remote/IndigenousCcrHttp.cpp
  src/cli/remote/IndigenousRemoteBinding.cpp src/cli/remote/IndigenousSSE.cpp
  src/cli/remote/IndigenousWebSocket.cpp src/cli/remote/SSECore.cpp
  src/cli/remote/SSEFrames.cpp src/cli/remote/SSEPost.cpp
  src/cli/remote/SSEReconnect.cpp src/cli/remote/SerialBatchEventUploader.cpp
  src/cli/remote/FlightToken.cpp src/cli/remote/StreamAccumulator.cpp
  src/cli/remote/UrlSearch.cpp src/cli/remote/WebSocketCore.cpp
  src/cli/remote/WebSocketProxy.cpp src/cli/remote/WebSocketReconnect.cpp
  src/cli/remote/WebSocketTimers.cpp src/cli/remote/WorkerStateUploader.cpp)
target_link_libraries(batbox_remote PUBLIC batbox_remote_io batbox_core
  batbox_event_loop batbox_url ada::ada CURL::libcurl
  PkgConfig::BATBOX_WEBSOCKETS ICU::uc)
target_compile_options(batbox_remote PRIVATE -Wall -Wextra -Wpedantic)
