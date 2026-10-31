add_library(batbox_sdk_events src/sdk/EventQueue.cpp src/sdk/IndigenousBindings.cpp
  src/flights/Lifecycle.cpp)
target_link_libraries(batbox_sdk_events PUBLIC batbox_core batbox_bats)
target_compile_options(batbox_sdk_events PRIVATE -Wall -Wextra -Wpedantic)
