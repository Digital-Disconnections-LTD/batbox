foreach(section IN ITEMS memory dynamic)
  add_executable(batbox-attachment_triggers_${section}-probe
    tests/phase5/attachment_triggers_${section}_probe.cpp)
  target_link_libraries(batbox-attachment_triggers_${section}-probe PRIVATE batbox_attachment_triggers)

endforeach()
batbox_indigenous(phase5-attachment-triggers-indigenous tests/phase5/attachment_triggers_indigenous.cpp
  batbox_attachment_triggers)
