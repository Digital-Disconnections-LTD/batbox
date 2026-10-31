# Indigenous endpoint catalog is shared by startup configuration and the FTXUI
# command panel. Discovery runs on a worker and uses the existing HTTP client.
target_sources(batbox_application_config PRIVATE
  src/application_config/EndpointStore.cpp
  src/application_compaction_settings/Policy.cpp)
target_link_libraries(batbox_application_config PUBLIC batbox_app_state batbox_event_loop)
target_sources(batbox_application_commands PRIVATE
  src/application_config/Discovery.cpp
  src/application_config/ConfigPanel.cpp
  src/application_config/ConfigPanelViews.cpp
  src/application_config/ConfigPanelEdit.cpp)
target_link_libraries(batbox_application_commands PUBLIC batbox_mcp_runtime)
if(BUILD_TESTING)
  add_executable(batbox-endpoint-config-indigenous
    tests/phase6/endpoint_config_indigenous.cpp)
  target_link_libraries(batbox-endpoint-config-indigenous PRIVATE
    batbox_application_commands batbox_application_config)
  target_compile_options(batbox-endpoint-config-indigenous PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
  add_test(NAME phase6-endpoint-config-indigenous
    COMMAND batbox-endpoint-config-indigenous)
endif()
