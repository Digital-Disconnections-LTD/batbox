foreach(section IN ITEMS web_core web_schema web_model web_policy)
  batbox_probe(${section} batbox_web phase3)
endforeach()
add_executable(batbox-web-gadgets tests/phase3/web_gadgets.cpp)
target_link_libraries(batbox-web-gadgets PRIVATE batbox_web)
add_test(NAME phase3-web-gadgets COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_SOURCE_DIR}/tests/phase3/web_gadgets.py --probe $<TARGET_FILE:batbox-web-gadgets>)
