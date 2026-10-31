foreach(section IN ITEMS
  files_edit_call
  files_edit_utils
  files_edit_validation
  files_glob
  files_grep
  files_notebook
  files_range
  files_read_call
  files_read_result
  files_read_utils
  files_schema
  files_write
)
  batbox_probe(${section} batbox_core phase3)
endforeach()
batbox_indigenous(phase3-files-factory tests/phase3/files_factory.cpp batbox_core)
add_executable(batbox-files-ripgrep tests/phase3/files_ripgrep_probe.cpp)
target_link_libraries(batbox-files-ripgrep PRIVATE batbox_core)
add_test(NAME phase3-files-ripgrep COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_SOURCE_DIR}/tests/phase3/files_ripgrep_integration.py --probe $<TARGET_FILE:batbox-files-ripgrep>)
