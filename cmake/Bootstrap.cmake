add_library(batbox_bootstrap_state
  src/bootstrap/CoreFlags.cpp
  src/bootstrap/FlightCron.cpp src/bootstrap/FlightTransitions.cpp src/bootstrap/PoetryState.cpp
  src/bootstrap/InvokedSkills.cpp src/bootstrap/IndigenousInvokedSkills.cpp)
target_link_libraries(batbox_bootstrap_state PUBLIC batbox_roost batbox_core batbox_event_loop)
target_compile_options(batbox_bootstrap_state PRIVATE -Wall -Wextra -Wpedantic)
# The legacy flight-restore shim (src/flight_restore/State.cpp) now compiles
# inside batbox_application_flights (IndigenousApplicationBackend.cmake).
