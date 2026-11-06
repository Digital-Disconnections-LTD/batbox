add_executable(batbox-web-gadgets tests/phase3/web_gadgets.cpp)
target_link_libraries(batbox-web-gadgets PRIVATE batbox_web)
add_test(NAME phase3-web-gadgets COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_SOURCE_DIR}/tests/phase3/web_gadgets.py --probe $<TARGET_FILE:batbox-web-gadgets>)
