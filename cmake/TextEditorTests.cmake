foreach(section IN ITEMS kill_ring measured_text cursor text_editor)
  add_executable(batbox-indigenous-${section}-probe tests/phase5/indigenous_${section}_probe.cpp)
  target_link_libraries(batbox-indigenous-${section}-probe PRIVATE batbox_text_editor)

endforeach()
batbox_indigenous(phase5-indigenous-text-editor-real tests/phase5/indigenous_text_editor_real.cpp
  "batbox_text_editor;batbox_interaction_controller")
batbox_probe(indigenous_input_buffer batbox_text_editor phase5)
