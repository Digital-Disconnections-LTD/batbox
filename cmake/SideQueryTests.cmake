foreach(section IN ITEMS side_query sdk_retry)
  add_executable(batbox-${section}-probe tests/phase5/${section}_probe.cpp)
  target_link_libraries(batbox-${section}-probe PRIVATE batbox_side_query)

endforeach()
add_executable(batbox-side-query-http-probe tests/phase5/side_query_http_probe.cpp)
target_link_libraries(batbox-side-query-http-probe PRIVATE batbox_memory_prefetch)
target_compile_options(batbox-side-query-http-probe PRIVATE -UNDEBUG)
add_test(NAME phase5-side-query-http COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_SOURCE_DIR}/tests/phase5/side_query_http.py
  --probe $<TARGET_FILE:batbox-side-query-http-probe>)
