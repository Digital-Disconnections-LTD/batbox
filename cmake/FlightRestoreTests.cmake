# Flight listing/recovery tests. The restore library itself folded into
# batbox_application_flights (IndigenousApplicationBackend.cmake).
if(BUILD_TESTING)
  add_executable(batbox-flight-listing-indigenous tests/phase5/flight_listing_indigenous.cpp)
  target_link_libraries(batbox-flight-listing-indigenous PRIVATE
    batbox_application_flights batbox_application_commands)
  target_compile_definitions(batbox-flight-listing-indigenous PRIVATE
    BATBOX_FLIGHT_FIXTURES="${CMAKE_SOURCE_DIR}/tests/fixtures/flights")
  add_test(NAME phase5-flight-listing-indigenous COMMAND batbox-flight-listing-indigenous)
  add_executable(batbox-flight-recovery-indigenous tests/phase6/flight_recovery_indigenous.cpp)
  target_link_libraries(batbox-flight-recovery-indigenous PRIVATE
    batbox_application_flights)
  add_test(NAME phase6-flight-recovery-indigenous COMMAND batbox-flight-recovery-indigenous)
endif()
