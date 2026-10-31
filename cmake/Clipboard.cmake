add_library(batbox_clipboard src/termio/Clipboard.cpp src/termio/ClipboardBinding.cpp)
target_link_libraries(batbox_clipboard PUBLIC batbox_termio batbox_process batbox_media)
target_compile_options(batbox_clipboard PRIVATE -Wall -Wextra -Wpedantic)
