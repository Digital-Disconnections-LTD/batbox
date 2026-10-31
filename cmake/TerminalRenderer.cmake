add_library(batbox_terminal_renderer
  src/terminal_renderer/Frame.cpp
  src/terminal_renderer/Renderer.cpp
)
target_link_libraries(batbox_terminal_renderer PUBLIC batbox_terminal_style batbox_terminal_screen batbox_terminal_output)
target_compile_options(batbox_terminal_renderer PRIVATE -Wall -Wextra -Wpedantic)
