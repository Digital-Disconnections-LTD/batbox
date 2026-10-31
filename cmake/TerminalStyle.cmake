add_library(batbox_terminal_style
  src/terminal_dom/style/StyleApply.cpp
  src/terminal_dom/style/StyleBox.cpp
  src/terminal_dom/style/StyleFlex.cpp
  src/terminal_dom/style/StyleValues.cpp
)
target_link_libraries(batbox_terminal_style PUBLIC batbox_terminal_dom)
target_compile_options(batbox_terminal_style PRIVATE -Wall -Wextra -Wpedantic)
