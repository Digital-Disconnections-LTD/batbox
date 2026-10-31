foreach(section IN ITEMS aggregate leaves)
  add_executable(batbox-attachments_${section}-probe
    tests/phase5/attachments_${section}_probe.cpp)
  target_link_libraries(batbox-attachments_${section}-probe PRIVATE batbox_attachments)

endforeach()
foreach(section IN ITEMS mentions collectors)
  add_executable(batbox-attachment_file_${section}-probe
    tests/phase5/attachment_file_${section}_probe.cpp)
  target_link_libraries(batbox-attachment_file_${section}-probe PRIVATE batbox_attachments)

endforeach()
batbox_indigenous(phase5-attachment-file-collectors-indigenous
  tests/phase5/attachment_file_collectors_indigenous.cpp batbox_attachments)
add_executable(batbox-attachment_context-probe tests/phase5/attachment_context_probe.cpp)
target_link_libraries(batbox-attachment_context-probe PRIVATE batbox_attachments)

batbox_indigenous(phase5-attachment-context-indigenous tests/phase5/attachment_context_indigenous.cpp batbox_attachments)
add_executable(batbox-attachment_local-probe tests/phase5/attachment_local_probe.cpp)
target_link_libraries(batbox-attachment_local-probe PRIVATE batbox_attachments)
