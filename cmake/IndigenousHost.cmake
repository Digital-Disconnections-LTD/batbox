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
set(BATBOX_SETUP_WEB_SOURCE ${CMAKE_SOURCE_DIR}/gadgets/setup-web.sh)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${BATBOX_SETUP_WEB_SOURCE})
file(READ ${BATBOX_SETUP_WEB_SOURCE} BATBOX_SETUP_WEB_SCRIPT)
configure_file(src/application/SetupWebScript.hpp.in
  ${CMAKE_BINARY_DIR}/generated/application/SetupWebScript.hpp @ONLY)
add_library(batbox_subcommands STATIC src/application/Subcommands.cpp)
target_include_directories(batbox_subcommands PRIVATE ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(batbox_subcommands PUBLIC batbox_application_config
  batbox_application_connectors)
target_compile_options(batbox_subcommands PRIVATE -Wall -Wextra)
add_executable(batbox src/application/main.cpp)
target_link_libraries(batbox PRIVATE batbox_indigenous_host batbox_subcommands)
if(MINGW)
  target_link_options(batbox PRIVATE -municode)
endif()
install(TARGETS batbox RUNTIME DESTINATION bin COMPONENT Runtime)
include(cmake/IndigenousLsp.cmake)
