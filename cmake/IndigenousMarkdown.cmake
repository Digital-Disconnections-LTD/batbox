include(cmake/CmarkDependency.cmake)
target_sources(batbox_interactive PRIVATE
  src/interactive/markdown/Lexer.cpp
  src/interactive/markdown/Format.cpp
  src/interactive/markdown/Table.cpp
  src/interactive/markdown/Highlight.cpp)
target_link_libraries(batbox_interactive PUBLIC PkgConfig::CMARK_GFM)
target_sources(batbox_terminal_ftxui PRIVATE
  src/interactive/markdown/AnsiProjection.cpp
  src/interactive/markdown/Blocks.cpp
  src/interactive/markdown/Renderer.cpp
  src/interactive/markdown/IndigenousRenderer.cpp
  src/interactive/message_list_view/MarkdownBinding.cpp)
target_link_libraries(batbox_terminal_ftxui PUBLIC batbox_npm_wrap_ansi)
