target_sources(batbox_structured_io PRIVATE
  src/cli/permissions/HookDecision.cpp src/cli/permissions/PermissionRace.cpp
  src/cli/structured/IndigenousSchemas.cpp src/cli/structured/StructuredFlight.cpp)
target_link_libraries(batbox_structured_io PUBLIC batbox_instruction_async batbox_permission_poetry)
add_library(batbox_remote_io src/cli/remote/RemoteIO.cpp
  src/cli/remote/RemoteIOOperations.cpp src/cli/remote/TransportSelection.cpp)
target_link_libraries(batbox_remote_io PUBLIC batbox_structured_io batbox_url)
target_compile_options(batbox_remote_io PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_conversation PRIVATE
  src/query_engine/NormalizeMessage.cpp src/query_engine/NormalizeBindings.cpp)
