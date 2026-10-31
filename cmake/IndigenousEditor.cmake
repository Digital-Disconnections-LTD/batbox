# Include after batbox_conversation_shell and batbox_history targets.
add_library(batbox_application_editor
  src/application_editor/IndigenousProcess.cpp
  src/application_editor/PoetryEditor.cpp
  src/application_editor/TranscriptFile.cpp)
target_link_libraries(batbox_application_editor PUBLIC batbox_history
  batbox_core)
target_compile_options(batbox_application_editor PRIVATE
  -Wall -Wextra -Wpedantic)
target_link_libraries(batbox_conversation_shell PUBLIC
  batbox_application_editor)
target_sources(batbox_conversation_shell PRIVATE
  src/interactive/conversation_shell/ShellTranscript.cpp)
