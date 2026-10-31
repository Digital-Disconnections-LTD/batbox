foreach(section IN ITEMS background_query background_attachments background_mission flight_backgrounding)
  add_executable(batbox-${section}-probe tests/phase5/${section}_probe.cpp)
  target_link_libraries(batbox-${section}-probe PRIVATE batbox_background_flight)

endforeach()
batbox_indigenous(phase5-background-composition tests/phase5/background_composition_probe.cpp
  batbox_background_flight)
