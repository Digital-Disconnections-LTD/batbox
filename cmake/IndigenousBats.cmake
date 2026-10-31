target_sources(batbox_bats PRIVATE src/bats/ResumeFilters.cpp)
# Indigenous application bat and permission providers. Include after the
# application engine, model and shell targets are declared.
add_library(batbox_application_bats
  src/application_bats/GeneralPurpose.cpp
  src/application_bats/BatServices.cpp
  src/application_bats/IndigenousQueryBridge.cpp
  src/application_bats/IndigenousBatCore.cpp
  src/application_bats/IndigenousBatGadget.cpp
  src/application_bats/IndigenousBatRunner.cpp
  src/application_bats/IndigenousBatMissions.cpp
  src/application_bats/IndigenousBatShutdown.cpp
  src/application_bats/IndigenousBatConversation.cpp
  src/application_bats/IndigenousBatLifetime.cpp
  src/application_bats/IndigenousBatInputs.cpp
  src/application_bats/IndigenousBatResume.cpp
  src/application_bats/IndigenousBatMessaging.cpp
  src/bats/IndigenousMissionOutput.cpp
  src/application_bats/IndigenousBatSkill.cpp
  src/application_bats/DefinitionsParse.cpp
  src/application_bats/DefinitionsDiscover.cpp
  src/application_bats/Memory.cpp
  src/application_bats/Worktree.cpp
  src/application_bats/WorktreeSetup.cpp
  src/application_bats/BuiltinDefinitions.cpp
  src/application_bats/BatMcp.cpp
  src/application_permissions/PermissionProvider.cpp)
target_link_libraries(batbox_application_bats PUBLIC
  batbox_application_engine batbox_application_model batbox_application_shell
  batbox_bats batbox_flights batbox_interactive batbox_settings batbox_glob
  batbox_roost batbox_web)
target_compile_options(batbox_application_bats PRIVATE -Wall -Wextra -Wpedantic)
