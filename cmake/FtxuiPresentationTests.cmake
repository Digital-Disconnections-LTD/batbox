batbox_indigenous(phase6-ftxui-scrollback-indigenous
  tests/phase6/terminal_ftxui_scrollback_indigenous.cpp batbox_terminal_ftxui)
batbox_indigenous(phase6-interactive-presentation-indigenous
  tests/phase6/interactive_presentation_indigenous.cpp batbox_terminal_ftxui)
batbox_indigenous(phase6-ftxui-unicode-indigenous
  tests/phase6/terminal_ftxui_unicode_indigenous.cpp batbox_terminal_ftxui)
batbox_indigenous(phase6-text-wrap-columns-indigenous
  tests/phase6/text_wrap_columns_indigenous.cpp batbox_terminal_ftxui)
batbox_indigenous(phase6-ftxui-selection-spaces-indigenous
  tests/phase6/terminal_ftxui_selection_spaces_indigenous.cpp batbox_terminal_ftxui)
batbox_indigenous(phase6-transcript-copy-indigenous
  tests/phase6/transcript_copy_indigenous.cpp batbox_terminal_ftxui)
batbox_indigenous(phase5-ftxui-driver-headless
  tests/phase5/indigenous_ftxui_driver_headless.cpp batbox_terminal_ftxui)
if(UNIX)
  add_test(NAME phase6-agent-progress-pty
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/checks/indigenous_agent_progress_smoke.py
      $<TARGET_FILE:batbox>)
  set_tests_properties(phase6-agent-progress-pty PROPERTIES TIMEOUT 180)
  add_test(NAME phase6-transcript-copy-pty
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/checks/indigenous_transcript_copy_smoke.py
      $<TARGET_FILE:batbox>)
  set_tests_properties(phase6-transcript-copy-pty PROPERTIES TIMEOUT 180)
  add_test(NAME phase6-bat-switch-pty
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/checks/indigenous_bat_switch_smoke.py
      $<TARGET_FILE:batbox>)
  set_tests_properties(phase6-bat-switch-pty PROPERTIES TIMEOUT 180)
  add_test(NAME phase6-nested-bat-pty
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/checks/indigenous_nested_bat_smoke.py
      $<TARGET_FILE:batbox>)
  set_tests_properties(phase6-nested-bat-pty PROPERTIES TIMEOUT 180)
  add_test(NAME phase6-escape-interrupt-pty
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/checks/indigenous_escape_interrupt_smoke.py
      $<TARGET_FILE:batbox>)
  set_tests_properties(phase6-escape-interrupt-pty PROPERTIES TIMEOUT 180)
  add_test(NAME phase6-queued-poetry-pty
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/checks/indigenous_queued_poetry_smoke.py
      $<TARGET_FILE:batbox>)
  set_tests_properties(phase6-queued-poetry-pty PROPERTIES TIMEOUT 240)
  add_test(NAME phase6-queued-wake-pty
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/checks/indigenous_queued_wake_smoke.py
      $<TARGET_FILE:batbox>)
  set_tests_properties(phase6-queued-wake-pty PROPERTIES TIMEOUT 240)
  add_test(NAME phase6-waiting-cancel-pty
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/checks/indigenous_waiting_cancel_smoke.py
      $<TARGET_FILE:batbox>)
  set_tests_properties(phase6-waiting-cancel-pty PROPERTIES TIMEOUT 240)
  add_test(NAME phase6-paste-terminal-roundtrip
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/checks/indigenous_paste_smoke.py
      $<TARGET_FILE:batbox> --scrapling-fixture $<TARGET_FILE:batbox-scrapling-fixture>)
  set_tests_properties(phase6-paste-terminal-roundtrip PROPERTIES TIMEOUT 180)
  add_test(NAME phase6-welcome-animation-pty
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/tests/phase6/welcome_animation_pty.py
      $<TARGET_FILE:batbox-phase6-conversation-shell-indigenous>)
  set_tests_properties(phase6-welcome-animation-pty PROPERTIES TIMEOUT 12)
  batbox_indigenous(phase5-ftxui-driver-pty
    tests/phase5/indigenous_ftxui_driver_pty.cpp batbox_terminal_ftxui)
  batbox_indigenous(phase5-ftxui-scrollback-pty
    tests/phase5/indigenous_ftxui_scrollback_pty.cpp batbox_terminal_ftxui)
  if(NOT APPLE)
    target_link_libraries(batbox-phase5-ftxui-driver-pty PRIVATE util)
    target_link_libraries(batbox-phase5-ftxui-scrollback-pty PRIVATE util)
  endif()
endif()
batbox_indigenous(phase5-command-ui-indigenous tests/phase5/indigenous_command_ui.cpp batbox_interactive)

include(cmake/TextEditorTests.cmake)
include(cmake/MessageListTests.cmake)
include(cmake/InteractiveContextTests.cmake)
include(cmake/InteractiveCardsTests.cmake)
