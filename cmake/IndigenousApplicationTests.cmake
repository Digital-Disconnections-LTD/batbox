batbox_indigenous(phase6-conversation-shell-indigenous
  tests/phase6/conversation_shell_indigenous.cpp batbox_conversation_shell)
batbox_indigenous(phase6-permission-provider-indigenous
  tests/phase6/permission_provider_indigenous.cpp batbox_application_permissions)
batbox_indigenous(phase6-startup-presentation-indigenous
  tests/phase6/interactive_presentation_providers_probe.cpp batbox_conversation_shell)
batbox_indigenous(phase6-application-runtime-indigenous
  tests/phase6/application_runtime_indigenous.cpp batbox_application_runtime)
add_executable(batbox-phase5-live-gadget-roster-indigenous tests/phase5/live_gadget_roster_indigenous.cpp)
target_link_libraries(batbox-phase5-live-gadget-roster-indigenous PRIVATE batbox_live_gadget_roster)
add_test(NAME phase5-live-gadget-roster-indigenous COMMAND batbox-phase5-live-gadget-roster-indigenous
  ${CMAKE_SOURCE_DIR}/tests/phase4/mcp_stdio_server.py)
add_executable(batbox-phase5-application-gadgets-indigenous tests/phase5/application_gadgets_indigenous.cpp)
target_link_libraries(batbox-phase5-application-gadgets-indigenous PRIVATE batbox_application_gadgets)
add_test(NAME phase5-application-gadgets-indigenous COMMAND batbox-phase5-application-gadgets-indigenous
  ${CMAKE_SOURCE_DIR}/tests/phase4/mcp_stdio_server.py
  ${CMAKE_SOURCE_DIR}/tests/phase3/web_fake_scrapling.py)
batbox_indigenous(phase5-remaining-commands-indigenous
  tests/phase5/remaining_commands_indigenous.cpp batbox_core_commands)

add_executable(batbox-application-model-http tests/phase6/application_model_http_probe.cpp)
target_link_libraries(batbox-application-model-http PRIVATE batbox_application_model)
add_test(NAME phase6-application-model-http COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_SOURCE_DIR}/tests/phase6/application_model_http.py --probe
  $<TARGET_FILE:batbox-application-model-http>)
batbox_indigenous(phase6-application-poetry-indigenous
  tests/phase6/application_poetry_indigenous.cpp batbox_application_poetry)
batbox_indigenous(phase6-application-presentation-indigenous
  tests/phase6/application_presentation_indigenous.cpp batbox_application_presentation)
add_executable(batbox-indigenous-mcp-connections tests/phase6/mcp_connections_indigenous.cpp)
target_link_libraries(batbox-indigenous-mcp-connections PRIVATE batbox_mcp_runtime)

batbox_indigenous(phase6-application-usage-indigenous
  tests/phase6/application_usage_indigenous.cpp batbox_application_commands)
if(UNIX)
  add_test(NAME phase6-indigenous-effort-smoke COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/checks/indigenous_effort_smoke.py $<TARGET_FILE:batbox>)
  set_tests_properties(phase6-indigenous-effort-smoke PROPERTIES TIMEOUT 70)
endif()
batbox_indigenous(phase6-indigenous-lsp-grouped checks/indigenous_lsp_grouped.cpp batbox_indigenous_lsp)
add_executable(batbox-indigenous-lsp-runtime checks/indigenous_lsp_runtime.cpp)
target_link_libraries(batbox-indigenous-lsp-runtime PRIVATE batbox_indigenous_lsp)
target_compile_options(batbox-indigenous-lsp-runtime PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
add_test(NAME phase6-indigenous-lsp-runtime COMMAND batbox-indigenous-lsp-runtime
  ${CMAKE_SOURCE_DIR}/checks/indigenous_lsp_fixture_server.py)
set_tests_properties(phase6-indigenous-lsp-runtime PROPERTIES TIMEOUT 40)
batbox_indigenous(phase6-indigenous-host-lifetime
  tests/phase6/indigenous_host_lifetime.cpp batbox_indigenous_host)
set_tests_properties(phase6-indigenous-host-lifetime PROPERTIES TIMEOUT 45)
add_executable(batbox-indigenous-lsp-format-driver checks/indigenous_lsp_format_driver.cpp)
target_link_libraries(batbox-indigenous-lsp-format-driver PRIVATE batbox_indigenous_lsp)
target_compile_options(batbox-phase6-indigenous-lsp-grouped PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
