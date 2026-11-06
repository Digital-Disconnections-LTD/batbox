add_library(batbox_bootstrap_state
  src/bootstrap/CoreFlags.cpp
  src/bootstrap/FlightCron.cpp src/bootstrap/FlightTransitions.cpp src/bootstrap/PoetryState.cpp
  src/bootstrap/InvokedSkills.cpp src/bootstrap/IndigenousInvokedSkills.cpp)
target_link_libraries(batbox_bootstrap_state PUBLIC batbox_roost batbox_core batbox_event_loop)
target_compile_options(batbox_bootstrap_state PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_flight_restore src/flight_restore/State.cpp)
target_link_libraries(batbox_flight_restore PUBLIC batbox_flights batbox_instruction_async batbox_poetryEntries batbox_roost)
target_compile_options(batbox_flight_restore PRIVATE -Wall -Wextra -Wpedantic)
