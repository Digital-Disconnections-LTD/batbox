add_library(batbox_headless_cli
  src/cli/headless/InterruptedMessage.cpp
  src/cli/headless/OrphanedPermission.cpp
  src/cli/headless/PermissionControl.cpp
  src/cli/headless/PoetryBatch.cpp
  src/cli/headless/UnresolvedGadgetUse.cpp
)
target_link_libraries(batbox_headless_cli PUBLIC batbox_flights batbox_bat_context)
target_compile_options(batbox_headless_cli PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_headless_cli PRIVATE src/cli/util/Exit.cpp src/cli/util/Ndjson.cpp)
target_link_libraries(batbox_headless_cli PUBLIC batbox_cleanup)
