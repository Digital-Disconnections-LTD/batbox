add_library(batbox_message_runtime src/messages/RuntimeMessages.cpp src/messages/Tag.cpp)
target_link_libraries(batbox_message_runtime PUBLIC batbox_core batbox_glob)
target_compile_options(batbox_message_runtime PRIVATE -Wall -Wextra -Wpedantic)

include(cmake/IndigenousTls.cmake)
add_library(batbox_application_model
  src/application_model/LocalModelService.cpp src/application_model/ServicesBase.cpp
  src/application_model/ServicesPreflight.cpp src/application_model/ServicesQuery.cpp
  src/application_model/ServicesReducer.cpp src/application_model/ServicesRequest.cpp
  src/application_model/ServicesRetry.cpp src/application_model/ContextImports.cpp
  src/application_model/Microcompact.cpp)
target_link_libraries(batbox_application_model PUBLIC batbox_query batbox_model
  batbox_diagnostics batbox_bats batbox_indigenous_tls batbox_wire_clients)
target_compile_options(batbox_application_model PRIVATE -Wall -Wextra -Wpedantic)

add_library(batbox_application_engine src/application_engine/Engine.cpp
  src/application_engine/StopCursor.cpp src/application_engine/Compactor.cpp
  src/application_engine/CompactModel.cpp src/application_engine/Execution.cpp
  src/application_engine/GadgetErrors.cpp
  src/application_engine/CompactBinding.cpp src/application_engine/CompactFork.cpp
  src/application_engine/Defaults.cpp)
target_link_libraries(batbox_application_engine PUBLIC batbox_application_config
  batbox_application_model batbox_query batbox_compaction batbox_gadget_results
  batbox_message_runtime batbox_interactive batbox_shell)
target_compile_options(batbox_application_engine PRIVATE -Wall -Wextra -Wpedantic)

# Flight bindings plus the flight-restore layer (formerly the separate
# application_restore target, sourced from this same directory) and the
# application roost host (formerly application_roost). These three were one
# session-flight module split into three cmake slices.
add_library(batbox_application_flights src/application_flights/Flight.cpp
  src/application_flights/Plans.cpp
  src/application_roost/Roost.cpp
  src/flight_restore/State.cpp
  src/application_flights/RestoreLoad.cpp
  src/application_flights/RestoreApply.cpp
  src/application_flights/RestoreCommand.cpp)
target_link_libraries(batbox_application_flights PUBLIC batbox_application_config
  batbox_flights batbox_process batbox_message_runtime batbox_core
  batbox_core_commands batbox_roost batbox_instruction_async
  batbox_poetryEntries batbox_shell batbox_gadget_results
  batbox_event_loop)
target_compile_options(batbox_application_flights PRIVATE -Wall -Wextra -Wpedantic)

add_library(batbox_application_shell src/application_shell/ApplicationShell.cpp
  src/application_shell/AsyncBashHost.cpp src/application_shell/RuleMatch.cpp)
target_link_libraries(batbox_application_shell PUBLIC batbox_shell batbox_shell_media
  batbox_gadget_results batbox_glob batbox_event_loop)
target_compile_options(batbox_application_shell PRIVATE -Wall -Wextra -Wpedantic)

add_library(batbox_application_connectors src/application_connectors/Bootstrap.cpp
  src/application_connectors/Status.cpp)
target_link_libraries(batbox_application_connectors PUBLIC batbox_application_config
  batbox_connectors batbox_mcp batbox_event_loop)
target_compile_options(batbox_application_connectors PRIVATE -Wall -Wextra -Wpedantic)

include(cmake/ProjectRecovery.cmake)
add_library(batbox_application_poetry src/application_poetry/ProviderCore.cpp
  src/application_poetry/ProviderRecovery.cpp
  src/application_poetry/ProviderGit.cpp src/application_poetry/ProviderContext.cpp
  src/application_poetry/ProviderPoetry.cpp src/application_poetry/ProviderHead.cpp)
target_link_libraries(batbox_application_poetry PUBLIC batbox_application_config
  batbox_poetryEntries batbox_context batbox_git_utils batbox_git batbox_instruction_async
  batbox_instructions batbox_glob batbox_process batbox_roost batbox_project_recovery)
target_compile_options(batbox_application_poetry PRIVATE -Wall -Wextra -Wpedantic)

include(cmake/IndigenousBats.cmake)
# PermissionProvider is a leaf provider: it binds shell rules, gadget
# views, and app-state helpers. Owning it as a real target (instead of an
# INTERFACE alias to batbox_application_bats) keeps engine and hooks off
# the transitive application_bats path, which made hooks->engine->bats
# cyclic through this alias.
add_library(batbox_application_permissions src/application_permissions/PermissionProvider.cpp)
target_link_libraries(batbox_application_permissions PUBLIC
  batbox_core batbox_app_state batbox_application_shell batbox_web batbox_interactive)
target_compile_options(batbox_application_permissions PRIVATE -Wall -Wextra -Wpedantic)

add_library(batbox_application_hooks src/application_hooks/ProviderCore.cpp
  src/application_hooks/ProviderDispatch.cpp src/application_hooks/ProviderExecute.cpp
  src/application_hooks/ProviderBindings.cpp
  src/application_hooks/PermissionDecision.cpp src/application_hooks/ProviderRegistration.cpp
  src/application_hooks/ProviderWorktree.cpp
  src/application_hooks/ProviderFlightStart.cpp
  src/application_hooks/ProviderAsync.cpp
  src/application_hooks/ProviderEnv.cpp
  src/application_hooks/ProviderHttp.cpp
  src/application_hooks/ProviderPoetry.cpp
  src/application_hooks/ProviderBat.cpp
  src/application_hooks/ProviderFunction.cpp
  src/application_hooks/FileChangedWatcher.cpp)
target_link_libraries(batbox_application_hooks PUBLIC batbox_application_permissions batbox_application_config
  batbox_instruction_async batbox_application_model batbox_query batbox_glob
  batbox_shell batbox_skills batbox_settings batbox_synthetic_output
  batbox_interactive)
target_compile_options(batbox_application_hooks PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(batbox_application_engine PUBLIC batbox_application_hooks
  batbox_application_poetry batbox_application_flights
  batbox_application_permissions
  batbox_attachments batbox_bootstrap_state batbox_memory_prefetch)

target_sources(batbox_application_gadgets PRIVATE src/application_gadgets/DefaultInputs.cpp)
# The file-history bindings (formerly their own application_file_history
# target) are gadget-side bindings over flight storage; merged here.
target_sources(batbox_application_gadgets PRIVATE
  src/application_file_history/HistoryState.cpp
  src/application_file_history/HistoryBackup.cpp
  src/application_file_history/HistorySnapshots.cpp
  src/application_file_history/HistoryRestore.cpp
  src/application_file_history/HistoryDiff.cpp
  src/application_file_history/HistoryResume.cpp)
target_link_libraries(batbox_application_gadgets PUBLIC batbox_application_model
  batbox_application_connectors batbox_application_shell batbox_application_bats
  batbox_application_permissions batbox_gadget_results batbox_settings
  batbox_application_flights batbox_core batbox_event_loop OpenSSL::Crypto)

# The presentation bindings (formerly application_presentation) and the
# rewind modal join the frontend bindings: one presentation-side module.
add_library(batbox_application_frontend src/application_frontend/Defaults.cpp
  src/application_frontend/RewindModal.cpp
  src/application_presentation/Bindings.cpp)
target_link_libraries(batbox_application_frontend PUBLIC batbox_application_runtime
  batbox_application_poetry batbox_application_flights batbox_application_bats
  batbox_application_permissions batbox_message_runtime batbox_application_gadgets
  batbox_conversation_shell batbox_text_editor batbox_text_sdk)
target_compile_options(batbox_application_frontend PRIVATE -Wall -Wextra -Wpedantic)

include(cmake/IndigenousCli.cmake)
include(cmake/IndigenousMemory.cmake)
include(cmake/IndigenousCommands.cmake)
include(cmake/IndigenousEndpointConfig.cmake)
include(cmake/IndigenousMissionUi.cmake)
include(cmake/MissionManagement.cmake)
include(cmake/FlightRestoreTests.cmake)
include(cmake/IndigenousMedia.cmake)
include(cmake/IndigenousShellShutdown.cmake)
include(cmake/IndigenousSkills.cmake)
include(cmake/IndigenousMcp.cmake)
include(cmake/IndigenousHost.cmake)
include(cmake/IndigenousElicitation.cmake)
