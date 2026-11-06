batbox_indigenous(phase3-files-factory tests/phase3/files_factory.cpp batbox_core)
add_executable(batbox-files-ripgrep tests/phase3/files_ripgrep_probe.cpp)
target_link_libraries(batbox-files-ripgrep PRIVATE batbox_core)
add_test(NAME phase3-files-ripgrep COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_SOURCE_DIR}/tests/phase3/files_ripgrep_integration.py --probe $<TARGET_FILE:batbox-files-ripgrep>)
