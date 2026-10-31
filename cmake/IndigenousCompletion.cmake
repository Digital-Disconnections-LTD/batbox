# Include after batbox_conversation_shell and batbox_user_input targets.
add_library(batbox_application_completion
  src/application_completion/Commands.cpp
  src/application_completion/Paths.cpp)
target_link_libraries(batbox_application_completion PUBLIC
  batbox_user_input batbox_event_loop batbox_core)
target_compile_options(batbox_application_completion PRIVATE
  -Wall -Wextra -Wpedantic)
target_link_libraries(batbox_conversation_shell PUBLIC
  batbox_application_completion)
