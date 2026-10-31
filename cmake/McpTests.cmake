foreach(section IN ITEMS pure result sanitize validation process elicitation call factory collapse storage)
  add_executable(batbox-mcp-${section}-probe tests/phase4/mcp_${section}_probe.cpp)
  target_link_libraries(batbox-mcp-${section}-probe PRIVATE batbox_mcp)

endforeach()
foreach(section IN ITEMS stdio factory)
  add_executable(batbox-mcp-${section}-indigenous tests/phase4/mcp_${section}_indigenous.cpp)
  target_link_libraries(batbox-mcp-${section}-indigenous PRIVATE batbox_mcp)
  add_test(NAME phase4-mcp-${section}-indigenous COMMAND batbox-mcp-${section}-indigenous
    ${CMAKE_SOURCE_DIR}/tests/phase4/mcp_stdio_server.py)
endforeach()
batbox_indigenous(phase4-mcp-auth-indigenous tests/phase4/mcp_auth_indigenous.cpp batbox_mcp)
