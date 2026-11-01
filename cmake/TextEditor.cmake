add_library(batbox_text_editor
  src/interactive/text_input/CursorCore.cpp
  src/interactive/text_input/CursorEdit.cpp
  src/interactive/text_input/CursorRender.cpp
  src/interactive/text_input/CursorWords.cpp
  src/interactive/text_input/KillRing.cpp
  src/interactive/text_input/InputBuffer.cpp
  src/interactive/text_input/InputModes.cpp
  src/interactive/text_input/MeasuredTextCore.cpp
  src/interactive/text_input/MeasuredTextLines.cpp
  src/interactive/text_input/MeasuredTextIndigenous.cpp
  src/interactive/text_input/TextEditorCore.cpp
  src/interactive/text_input/TextEditorKeys.cpp
  src/interactive/text_input/TextEditorIndigenous.cpp)
target_link_libraries(batbox_text_editor PUBLIC batbox_event_loop batbox_intl
  batbox_text_width batbox_npm_wrap_ansi batbox_terminal_events batbox_keyboard_input
  batbox_history)
target_compile_options(batbox_text_editor PRIVATE -Wall -Wextra -Wpedantic)
