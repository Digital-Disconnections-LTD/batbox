# Include after batbox_application_frontend and conversation_shell targets.
add_library(batbox_application_history
  src/application_history/History.cpp)
target_link_libraries(batbox_application_history PUBLIC
  batbox_history batbox_paste_store batbox_interactive
  batbox_fs_operations batbox_locking batbox_cleanup)
target_compile_options(batbox_application_history PRIVATE
  -Wall -Wextra -Wpedantic)
target_link_libraries(batbox_conversation_shell PUBLIC
  batbox_application_history)
target_link_libraries(batbox_application_frontend PUBLIC
  batbox_application_history)
