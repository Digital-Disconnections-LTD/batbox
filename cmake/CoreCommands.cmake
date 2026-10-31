add_library(batbox_core_commands
  src/commands_indigenous/Stashes.cpp
  src/commands_indigenous/Clear.cpp
  src/commands_indigenous/Compact.cpp
  src/commands_indigenous/CoreCatalog.cpp
  src/commands_indigenous/CoreHelpers.cpp
  src/commands_indigenous/HelpModel.cpp
  src/commands_indigenous/Resume.cpp)
target_link_libraries(batbox_core_commands PUBLIC batbox_user_input
  batbox_interactive batbox_compaction batbox_query batbox_app_state
  batbox_event_loop batbox_core)
target_compile_options(batbox_core_commands PRIVATE -Wall -Wextra -Wpedantic)
