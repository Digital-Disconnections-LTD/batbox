foreach(section IN ITEMS indigenous_style indigenous_style_yoga)
  add_executable(batbox-${section}-probe tests/phase5/${section}_probe.cpp)
  target_link_libraries(batbox-${section}-probe PRIVATE batbox_terminal_renderer)

endforeach()
batbox_probe(indigenous_frame batbox_terminal_renderer phase5)
batbox_probe(indigenous_renderer batbox_terminal_renderer phase5)
