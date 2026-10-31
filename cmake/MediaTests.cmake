foreach(section IN ITEMS image pdf)
  add_executable(batbox-media-${section}-probe tests/phase3/media_${section}_probe.cpp)
  target_link_libraries(batbox-media-${section}-probe PRIVATE batbox_media)

endforeach()
add_executable(batbox-media-indigenous tests/phase3/media_indigenous.cpp)
target_link_libraries(batbox-media-indigenous PRIVATE batbox_media)
add_test(NAME phase3-media-indigenous COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_SOURCE_DIR}/tests/phase3/media_indigenous.py --probe $<TARGET_FILE:batbox-media-indigenous>)
