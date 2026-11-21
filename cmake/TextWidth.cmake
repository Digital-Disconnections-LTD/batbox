include(cmake/TerminalTextSdk.cmake)
# Terminal control sequences (tokenizer, CSI, DEC modes, clear, OSC links)
# and clipboard bindings. Text measurement/wrapping lives in
# batbox_text_sdk (TerminalTextSdk.cmake).
add_library(batbox_termio src/termio/Tokenizer.cpp)
target_link_libraries(batbox_termio PUBLIC batbox_core)
target_compile_options(batbox_termio PRIVATE -Wall -Wextra -Wpedantic)
