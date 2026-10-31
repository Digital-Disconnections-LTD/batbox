add_library(batbox_process_output src/cli/process_output/ProcessOutput.cpp)
target_link_libraries(batbox_process_output PUBLIC batbox_fs_operations batbox_event_loop)
target_compile_options(batbox_process_output PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_permission_poetry src/cli/permissions/PoetryDecision.cpp src/cli/permissions/PoetryHelpers.cpp)
target_link_libraries(batbox_permission_poetry PUBLIC batbox_core)
target_compile_options(batbox_permission_poetry PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_terminal_hooks
  src/terminal_hooks/Notification.cpp src/terminal_hooks/Title.cpp
  src/terminal_hooks/TabStatus.cpp src/terminal_hooks/IndigenousServices.cpp)
target_link_libraries(batbox_terminal_hooks PUBLIC batbox_termio batbox_cleanup batbox_npm_wrap_ansi)
target_compile_options(batbox_terminal_hooks PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_query_engine_state PRIVATE src/query_engine/SystemInit.cpp)
