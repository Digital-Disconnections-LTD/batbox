# Concrete owners and providers used by the indigenous terminal application.
# The conversation shell and its input/editing/history bindings now compile
# inside the UI library pair (batbox_terminal_ftxui / batbox_interactive);
# see cmake/FtxuiPresentation.cmake.

target_sources(batbox_core_commands PRIVATE
  src/commands_indigenous/Rename.cpp src/commands_indigenous/FlightSimple.cpp)
target_link_libraries(batbox_core_commands PUBLIC batbox_monster batbox_bats)

add_library(batbox_live_gadget_roster src/gadgets/live_roster/Factory.cpp
  src/gadgets/live_roster/Select.cpp)
target_link_libraries(batbox_live_gadget_roster PUBLIC batbox_interactive
  batbox_bats batbox_shell batbox_core batbox_web batbox_mcp)
target_compile_options(batbox_live_gadget_roster PRIVATE -Wall -Wextra -Wpedantic)

add_library(batbox_application_config src/application/Config.cpp)
target_link_libraries(batbox_application_config PUBLIC batbox_core)
target_compile_options(batbox_application_config PRIVATE -Wall -Wextra -Wpedantic)

add_library(batbox_application_gadgets
  src/application_gadgets/ApplicationGadgets.cpp
  src/application_gadgets/IndigenousFiles.cpp src/application_gadgets/IndigenousFileHelpers.cpp)
target_link_libraries(batbox_application_gadgets PUBLIC batbox_live_gadget_roster
  batbox_skills batbox_media batbox_core batbox_fs_operations)
target_compile_options(batbox_application_gadgets PRIVATE -Wall -Wextra -Wpedantic)

add_library(batbox_application_runtime
  src/application_runtime/ApplicationRuntimeCore.cpp
  src/application_runtime/ApplicationBindings.cpp
  src/application_runtime/RuntimeBindings.cpp
  src/application_runtime/ShellBinding.cpp)
target_link_libraries(batbox_application_runtime PUBLIC batbox_terminal_ftxui
  batbox_application_config)
target_compile_options(batbox_application_runtime PRIVATE -Wall -Wextra -Wpedantic)
include(cmake/IndigenousApplicationBackend.cmake)
include(cmake/Providers.cmake)
