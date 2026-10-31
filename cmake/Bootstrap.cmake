add_library(batbox_bootstrap_identity src/bootstrap/Identity.cpp)
target_sources(batbox_bootstrap_identity PRIVATE src/bootstrap/PlanFlightState.cpp)
target_link_libraries(batbox_bootstrap_identity PUBLIC batbox_roost batbox_core)
target_compile_options(batbox_bootstrap_identity PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_bootstrap_state
  src/bootstrap/Costs.cpp src/bootstrap/Usage.cpp src/bootstrap/CostRestore.cpp
  src/bootstrap/IndigenousCosts.cpp src/bootstrap/Interaction.cpp src/bootstrap/IndigenousInteraction.cpp
  src/bootstrap/ModelState.cpp src/bootstrap/CoreFlags.cpp src/bootstrap/RequestState.cpp
  src/bootstrap/FlightCron.cpp src/bootstrap/FlightTransitions.cpp src/bootstrap/PoetryState.cpp
  src/bootstrap/ChannelState.cpp src/bootstrap/InvokedSkills.cpp src/bootstrap/IndigenousInvokedSkills.cpp
  src/bootstrap/SlowOperations.cpp src/bootstrap/IndigenousSlowOperations.cpp src/bootstrap/HookRoster.cpp
  src/bootstrap/BootstrapState.cpp src/bootstrap/IndigenousBootstrapState.cpp)
target_link_libraries(batbox_bootstrap_state PUBLIC batbox_bootstrap_identity batbox_event_loop)
target_compile_options(batbox_bootstrap_state PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_bootstrap_bindings src/bootstrap/BootstrapBindings.cpp)
target_link_libraries(batbox_bootstrap_bindings PUBLIC batbox_bootstrap_state batbox_instruction_async)
target_compile_options(batbox_bootstrap_bindings PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_flight_restore src/flight_restore/State.cpp src/flight_restore/BatMode.cpp
  src/flight_restore/Worktree.cpp src/flight_restore/Process.cpp src/flight_restore/IndigenousBinding.cpp
  src/flight_restore/WorktreeFlightState.cpp)
target_link_libraries(batbox_flight_restore PUBLIC batbox_flights batbox_instruction_async batbox_poetryEntries batbox_roost)
target_compile_options(batbox_flight_restore PRIVATE -Wall -Wextra -Wpedantic)
