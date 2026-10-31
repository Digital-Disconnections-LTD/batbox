foreach(section IN ITEMS permission_hook_decision permission_race structured_indigenous_schemas
    remote_io transport_selection)
  add_executable(batbox-${section}-probe tests/phase5/${section}_probe.cpp)
  target_link_libraries(batbox-${section}-probe PRIVATE batbox_remote_io)

endforeach()
batbox_indigenous(phase5-structured-flight-indigenous tests/phase5/structured_flight_indigenous.cpp batbox_structured_io)
batbox_probe(query_engine_normalize batbox_conversation phase5)
