foreach(section IN ITEMS message_state loading bash_gadgets initial_message bat_submit)
  add_executable(batbox-repl_${section}-probe tests/phase5/repl_${section}_probe.cpp)
  target_link_libraries(batbox-repl_${section}-probe PRIVATE batbox_interactive_lifecycle)

endforeach()
batbox_indigenous(phase5-repl-lifecycle-composition
  tests/phase5/repl_lifecycle_composition_probe.cpp batbox_interactive_lifecycle)
batbox_indigenous(phase5-repl-navigation-composition
  tests/phase5/navigation_composition_probe.cpp batbox_interactive_lifecycle)
batbox_indigenous(phase6-repl-runtime-indigenous
  tests/phase6/repl_runtime_indigenous.cpp batbox_interactive_lifecycle)
