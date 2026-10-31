# Include after batbox_structured_io and batbox_application_runtime exist.
add_library(batbox_application_cli src/application_cli/Adapter.cpp
  src/application_cli/LocalOutput.cpp
  src/cli/headless/PermissionControl.cpp)
target_link_libraries(batbox_application_cli PUBLIC
  batbox_structured_io batbox_application_runtime batbox_core batbox_npm_wrap_ansi)
target_compile_options(batbox_application_cli PRIVATE -Wall -Wextra -Wpedantic)
