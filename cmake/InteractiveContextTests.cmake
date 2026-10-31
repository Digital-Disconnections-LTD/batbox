foreach(section IN ITEMS context_factory context_gadget_pool typed_bat_state)
  add_executable(batbox-indigenous-${section}-probe tests/phase5/indigenous_${section}_probe.cpp)
  target_link_libraries(batbox-indigenous-${section}-probe PRIVATE batbox_interactive)

endforeach()
batbox_indigenous(phase5-indigenous-context-typed-bat tests/phase5/indigenous_context_typed_bat.cpp
  batbox_bats)
