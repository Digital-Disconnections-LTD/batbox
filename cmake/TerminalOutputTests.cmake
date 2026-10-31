foreach(section IN ITEMS terminal_output_queue terminal_output_render terminal_bidi)
  batbox_probe(${section} batbox_terminal_output phase6)
endforeach()
