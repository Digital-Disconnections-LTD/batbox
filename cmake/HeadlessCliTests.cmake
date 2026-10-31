foreach(section IN ITEMS print_poetry_batch print_orphan_permission print_unresolved_gadget print_permission_control print_interrupted_message)
  add_executable(batbox-${section}-probe tests/phase5/${section}_probe.cpp)
  target_link_libraries(batbox-${section}-probe PRIVATE batbox_headless_cli)

endforeach()
batbox_indigenous(phase5-print-unresolved-gadget-indigenous tests/phase5/print_unresolved_gadget_indigenous.cpp batbox_headless_cli)
batbox_probe(cli_util batbox_headless_cli phase5)
