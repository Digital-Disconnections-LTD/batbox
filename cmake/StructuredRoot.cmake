add_library(batbox_structured_io
  src/cli/structured/ControlMessageCompat.cpp
  src/cli/structured/ProcessLine.cpp
  src/cli/structured/InputReader.cpp
  src/cli/structured/OutputWrite.cpp
)
target_link_libraries(batbox_structured_io PUBLIC batbox_headless_cli batbox_event_loop)
target_compile_options(batbox_structured_io PRIVATE -Wall -Wextra -Wpedantic)
