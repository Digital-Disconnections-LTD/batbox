add_executable(batbox-termio_parser-probe tests/phase5/termio_parser_probe.cpp)
target_link_libraries(batbox-termio_parser-probe PRIVATE batbox_terminal_parser)

foreach(section IN ITEMS termio_style_equal termio_ansi_leaf)
  add_executable(batbox-${section}-probe tests/phase5/${section}_probe.cpp)
  target_link_libraries(batbox-${section}-probe PRIVATE batbox_terminal_parser)

endforeach()
