add_executable(batbox-file_state_stash-probe tests/phase5/file_state_stash_probe.cpp)
target_link_libraries(batbox-file_state_stash-probe PRIVATE batbox_core)

batbox_indigenous(phase5-file-state-stash-bindings tests/phase5/file_state_stash_bindings.cpp
  batbox_core)
batbox_indigenous(phase5-file-state-stash-bat-bindings
  tests/phase5/file_state_stash_bat_bindings.cpp batbox_bats)
add_executable(batbox-memory_attachment_filter-probe
  tests/phase5/memory_attachment_filter_probe.cpp)
target_link_libraries(batbox-memory_attachment_filter-probe PRIVATE batbox_query)
