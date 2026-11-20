target_sources(batbox_bats PRIVATE src/bats/ResumeFilters.cpp)
# Indigenous application bat and permission providers. Include after the
# application engine, model and shell targets are declared.
target_sources(batbox_bats PRIVATE src/application/bats/IndigenousQueryBridge.cpp)
# The query bridge is a thin adapter over query iterators plus a factory
# callback; its translation unit needs only query/event_loop/core headers.
# Compiling it into batbox_bats (an L2 target that never links the engine)
# lets batbox_application_engine use the bridge without linking
# batbox_application_bats -- breaking the engine<->application_bats cycle.
add_library(batbox_application_bats
  src/application/bats/GeneralPurpose.cpp
  src/application/bats/BatServices.cpp
  src/application/bats/IndigenousBatCore.cpp
  src/application/bats/IndigenousBatGadget.cpp
  src/application/bats/IndigenousBatRunner.cpp
  src/application/bats/IndigenousBatMissions.cpp
  src/application/bats/IndigenousBatShutdown.cpp
  src/application/bats/IndigenousBatConversation.cpp
  src/application/bats/IndigenousBatLifetime.cpp
  src/application/bats/IndigenousBatInputs.cpp
  src/application/bats/IndigenousBatResume.cpp
  src/application/bats/IndigenousBatMessaging.cpp
  src/bats/IndigenousMissionOutput.cpp
  src/application/bats/IndigenousBatSkill.cpp
  src/application/bats/DefinitionsParse.cpp
  src/application/bats/DefinitionsDiscover.cpp
  src/application/bats/Memory.cpp
  src/application/bats/Worktree.cpp
  src/application/bats/WorktreeSetup.cpp
  src/application/bats/BuiltinDefinitions.cpp
  src/application/bats/BatMcp.cpp)
target_link_libraries(batbox_application_bats PUBLIC
  batbox_application_engine batbox_application_model batbox_application_shell
  batbox_application_permissions
  batbox_bats batbox_flights batbox_interactive batbox_settings batbox_glob
  batbox_roost batbox_web)
target_compile_options(batbox_application_bats PRIVATE -Wall -Wextra -Wpedantic)
