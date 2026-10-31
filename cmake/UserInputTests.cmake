foreach(section IN ITEMS user_input_commands user_input_text user_input_slash
    user_input_pipeline user_input_bash user_input_fork user_input_immediate)
  add_executable(batbox-${section}-probe tests/phase5/${section}_probe.cpp)
  target_link_libraries(batbox-${section}-probe PRIVATE batbox_user_input)

endforeach()
batbox_indigenous(phase5-user-input-ui-indigenous tests/phase5/user_input_ui_indigenous.cpp batbox_user_input)
include(cmake/PoetrySubmitTests.cmake)
