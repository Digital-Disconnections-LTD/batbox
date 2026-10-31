add_library(batbox_mailbox src/mailbox/Mailbox.cpp src/mailbox/Bridge.cpp)
target_link_libraries(batbox_mailbox PUBLIC batbox_event_loop)
target_compile_options(batbox_mailbox PRIVATE -Wall -Wextra -Wpedantic)
