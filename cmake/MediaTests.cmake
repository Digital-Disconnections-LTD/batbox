add_executable(batbox-media-indigenous tests/phase3/media_indigenous.cpp)
target_link_libraries(batbox-media-indigenous PRIVATE batbox_media)
add_test(NAME phase3-media-indigenous COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_SOURCE_DIR}/tests/phase3/media_indigenous.py --probe $<TARGET_FILE:batbox-media-indigenous>)
set_tests_properties(phase3-media-indigenous PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP: the PDF part")
