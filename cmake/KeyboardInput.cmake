add_library(batbox_keyboard_input src/keyboard_input/KeyHelpers.cpp
  src/keyboard_input/KeyNames.cpp src/keyboard_input/Keypress.cpp
  src/keyboard_input/Responses.cpp src/keyboard_input/Multiple.cpp)
target_link_libraries(batbox_keyboard_input PUBLIC batbox_glob batbox_termio)
target_compile_options(batbox_keyboard_input PRIVATE -Wall -Wextra -Wpedantic)
