# MCP requests open an Indigenous dialog on the owner thread. The
# transport continues reading while its deferred response waits for the user.
target_sources(batbox_application_hooks PRIVATE
  src/application_hooks/ProviderElicitation.cpp)

add_library(batbox_application_mcp_elicitation
  src/application_mcp_elicitation/Controller.cpp
  src/application_mcp_elicitation/Validation.cpp
  src/application_mcp_elicitation/DateTimeParser.cpp)
target_link_libraries(batbox_application_mcp_elicitation PUBLIC
  batbox_interactive batbox_event_loop batbox_core)
target_compile_options(batbox_application_mcp_elicitation PRIVATE
  -Wall -Wextra -Wpedantic)
target_link_libraries(batbox_indigenous_host PUBLIC
  batbox_application_mcp_elicitation)

target_sources(batbox_application_mcp_elicitation PRIVATE
  src/application_mcp_elicitation/ModelBinding.cpp)
target_link_libraries(batbox_application_mcp_elicitation PUBLIC
  batbox_application_model)
if(BUILD_TESTING)
  add_executable(batbox-indigenous-mcp-elicitation tests/phase6/indigenous_mcp_elicitation.cpp)
  target_link_libraries(batbox-indigenous-mcp-elicitation PRIVATE
    batbox_application_mcp_elicitation batbox_mcp)
  add_test(NAME phase6-indigenous-mcp-elicitation COMMAND batbox-indigenous-mcp-elicitation)
  set_tests_properties(phase6-indigenous-mcp-elicitation PROPERTIES TIMEOUT 45)
  add_executable(batbox-indigenous-elicitation-date-format-driver
    checks/indigenous_elicitation_date_format_driver.cpp)
  target_link_libraries(batbox-indigenous-elicitation-date-format-driver PRIVATE
    batbox_application_mcp_elicitation)

endif()

if(BUILD_TESTING)

endif()
