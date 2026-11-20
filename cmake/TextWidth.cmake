include(cmake/TerminalTextSdk.cmake)
# Terminal control sequences (tokenizer, CSI, DEC modes, clear, OSC links)
# and clipboard bindings. Text measurement/wrapping lives in
# batbox_text_sdk (TerminalTextSdk.cmake).
add_library(batbox_termio src/termio/Tokenizer.cpp src/termio/Csi.cpp src/termio/Dec.cpp
  src/termio/Clear.cpp src/termio/OscLink.cpp)
target_link_libraries(batbox_termio PUBLIC batbox_core)
target_compile_options(batbox_termio PRIVATE -Wall -Wextra -Wpedantic)
