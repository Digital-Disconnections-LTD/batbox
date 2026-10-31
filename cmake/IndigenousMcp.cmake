# MCP authentication, connection state machines, and the Indigenous SDK transport boundary.
add_library(batbox_indigenous_jsonl_process src/process/IndigenousJsonlProcess.cpp)
target_link_libraries(batbox_indigenous_jsonl_process PUBLIC batbox_core
  PkgConfig::LIBUV Threads::Threads)
target_compile_options(batbox_indigenous_jsonl_process PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(batbox_mcp PUBLIC batbox_indigenous_jsonl_process)
target_link_libraries(batbox_connectors PUBLIC batbox_indigenous_jsonl_process)
if(WIN32)
  target_link_libraries(batbox_instruction_async PUBLIC batbox_indigenous_jsonl_process ws2_32)
  target_link_libraries(batbox_model_stash PUBLIC batbox_indigenous_jsonl_process)
  target_link_libraries(batbox_profiler PUBLIC psapi)
  target_link_libraries(batbox_media PUBLIC batbox_indigenous_jsonl_process)
endif()
if(WIN32)
  target_link_libraries(batbox_core PUBLIC bcrypt)
  target_link_libraries(batbox_core PUBLIC batbox_indigenous_jsonl_process)
endif()

add_library(batbox_mcp_runtime
  src/mcp/http/IndigenousHttp.cpp
  src/mcp/auth/Helpers.cpp src/mcp/auth/Provider.cpp src/mcp/auth/Refresh.cpp
  src/mcp/auth/Revocation.cpp
  src/mcp/auth/Discovery.cpp src/mcp/auth/OAuthFlight.cpp
  src/mcp/auth/IndigenousServices.cpp src/mcp/auth/Flow.cpp
  src/mcp/remote/RemoteCore.cpp src/mcp/remote/RemoteRequest.cpp
  src/mcp/remote/RemoteRead.cpp
  src/mcp/manager/ManagerCore.cpp src/mcp/manager/ManagerConnect.cpp
  src/mcp/manager/ManagerEvents.cpp src/mcp/manager/IndigenousRuntime.cpp
  src/mcp/manager/IndigenousConfiguration.cpp src/mcp/manager/IndigenousFactory.cpp
  src/mcp/manager/IndigenousAuth.cpp)
target_link_libraries(batbox_mcp_runtime PUBLIC batbox_mcp batbox_auth
  batbox_process batbox_url batbox_event_loop batbox_transport CURL::libcurl OpenSSL::Crypto)
target_compile_options(batbox_mcp_runtime PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(batbox_application_connectors PUBLIC batbox_mcp_runtime)
target_link_libraries(batbox_application_bats PUBLIC batbox_mcp_runtime)
