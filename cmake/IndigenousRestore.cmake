add_library(batbox_application_restore
  src/application_flights/RestoreLoad.cpp
  src/application_flights/RestoreApply.cpp
  src/application_flights/RestoreCommand.cpp)
target_link_libraries(batbox_application_restore PUBLIC
  batbox_application_flights batbox_core_commands batbox_flight_restore
  batbox_gadget_results batbox_core batbox_process batbox_shell batbox_application_roost)
target_compile_options(batbox_application_restore PRIVATE -Wall -Wextra -Wpedantic)
if(BUILD_TESTING)
  add_executable(batbox-flight-recovery-indigenous tests/phase6/flight_recovery_indigenous.cpp)
  target_link_libraries(batbox-flight-recovery-indigenous PRIVATE
    batbox_application_flights batbox_flight_restore)

endif()
