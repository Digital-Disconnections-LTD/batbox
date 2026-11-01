add_library(batbox_indigenous_host STATIC
  src/application/HostCore.cpp
  src/application/HostBackend.cpp src/application/HostFrontend.cpp
  src/application/ModelBinding.cpp src/application/HostMaintenance.cpp
  src/application/HostShutdown.cpp)
target_link_libraries(batbox_indigenous_host PUBLIC
  batbox_profiler
  batbox_application_config batbox_application_model
  batbox_application_engine batbox_application_flights
  batbox_application_shell batbox_application_connectors
  batbox_application_poetry batbox_application_bats
  batbox_application_hooks batbox_application_permissions
  batbox_application_gadgets batbox_application_runtime
  batbox_application_frontend batbox_application_presentation
  batbox_application_commands batbox_application_restore batbox_application_cli
  batbox_application_memory)
target_compile_options(batbox_indigenous_host PRIVATE -Wall -Wextra -Wpedantic)
add_executable(batbox src/application/main.cpp)
target_link_libraries(batbox PRIVATE batbox_indigenous_host)
if(MINGW)
  target_link_options(batbox PRIVATE -municode)
endif()
install(TARGETS batbox RUNTIME DESTINATION bin COMPONENT Runtime)
include(cmake/IndigenousLsp.cmake)
