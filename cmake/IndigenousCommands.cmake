add_library(batbox_application_commands src/application/commands/Commands.cpp
  src/application/commands/CommandsMcp.cpp src/application/commands/MemoryPanel.cpp
  src/application/commands/PermissionCommand.cpp
  src/application/commands/ProjectCommand.cpp src/application/commands/PoetryCommand.cpp
  src/application/commands/EffortCommand.cpp src/application/commands/ThemeCommand.cpp
  src/application/commands/ConnectorsCommand.cpp
  src/application/commands/ProvidersCommand.cpp
  src/application/usage/UsageCommand.cpp src/application/commands/FlightTitles.cpp)
target_link_libraries(batbox_application_commands PUBLIC
  batbox_core_commands batbox_application_runtime batbox_application_model
  batbox_application_poetry batbox_application_flights
  batbox_terminal_ftxui batbox_user_input
  batbox_application_memory batbox_bootstrap_state
  batbox_flight_titles batbox_application_connectors batbox_providers)
target_compile_options(batbox_application_commands PRIVATE -Wall -Wextra -Wpedantic)
