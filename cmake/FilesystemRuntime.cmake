target_sources(batbox_diagnostics PRIVATE src/diagnostics/LogDates.cpp)
add_library(batbox_fs_operations src/fs_operations/PathChecks.cpp
  src/fs_operations/IndigenousPathChecks.cpp)
target_link_libraries(batbox_fs_operations PUBLIC batbox_core batbox_event_loop)
target_compile_options(batbox_fs_operations PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_fs_operations PRIVATE
  src/fs_operations/NodeFsAsyncIndigenous.cpp
  src/fs_operations/NodeFsBytesIndigenous.cpp
  src/fs_operations/ReverseLines.cpp)
