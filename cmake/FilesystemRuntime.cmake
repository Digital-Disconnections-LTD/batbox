add_library(batbox_stash_paths src/stash_paths/StashPaths.cpp src/stash_paths/IndigenousJoin.cpp)
target_link_libraries(batbox_stash_paths PUBLIC batbox_core)
target_compile_options(batbox_stash_paths PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_diagnostics PRIVATE src/diagnostics/LogDates.cpp
  src/diagnostics/LogBrowser.cpp src/diagnostics/LogBrowserIndigenous.cpp
  src/diagnostics/SlowOperations.cpp src/diagnostics/SlowFileIndigenous.cpp
  src/diagnostics/ExecSyncWrapper.cpp)
add_library(batbox_fs_operations src/fs_operations/PathChecks.cpp
  src/fs_operations/IndigenousPathChecks.cpp)
target_link_libraries(batbox_fs_operations PUBLIC batbox_core batbox_event_loop)
target_compile_options(batbox_fs_operations PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_fs_operations PRIVATE
  src/fs_operations/NodeFsSync.cpp src/fs_operations/NodeFsIndigenous.cpp
  src/fs_operations/NodeFsAsync.cpp src/fs_operations/NodeFsAsyncIndigenous.cpp
  src/fs_operations/NodeFsBytes.cpp src/fs_operations/NodeFsBytesIndigenous.cpp
  src/fs_operations/FileRanges.cpp src/fs_operations/ReverseLines.cpp
  src/fs_operations/FsRoster.cpp)
target_link_libraries(batbox_fs_operations PUBLIC batbox_diagnostics)
target_sources(batbox_fs_operations PRIVATE src/fs_operations/IndigenousWritableStream.cpp)
