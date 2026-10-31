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
  batbox_diagnostics batbox_bats batbox_indigenous_tls)
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

add_library(batbox_application_roost src/application_roost/Roost.cpp)
target_link_libraries(batbox_application_roost PUBLIC batbox_core batbox_event_loop)
target_compile_options(batbox_application_roost PRIVATE -Wall -Wextra -Wpedantic)

add_library(batbox_application_flights src/application_flights/Flight.cpp
  src/application_flights/Plans.cpp)
target_link_libraries(batbox_application_flights PUBLIC batbox_application_config
  batbox_flights batbox_process batbox_message_runtime batbox_application_roost)
target_compile_options(batbox_application_flights PRIVATE -Wall -Wextra -Wpedantic)

add_library(batbox_application_shell src/application_shell/ApplicationShell.cpp
  src/application_shell/AsyncBashHost.cpp src/application_shell/RuleMatch.cpp)
target_link_libraries(batbox_application_shell PUBLIC batbox_shell batbox_shell_media
  batbox_gadget_results batbox_glob batbox_event_loop)
target_compile_options(batbox_application_shell PRIVATE -Wall -Wextra -Wpedantic)

add_library(batbox_application_connectors src/application_connectors/Bootstrap.cpp)
target_link_libraries(batbox_application_connectors PUBLIC batbox_application_config
  batbox_connectors batbox_mcp batbox_event_loop)
target_compile_options(batbox_application_connectors PRIVATE -Wall -Wextra -Wpedantic)

include(cmake/ProjectRecovery.cmake)
add_library(batbox_application_poetry src/application_poetry/ProviderCore.cpp
  src/application_poetry/ProviderRecovery.cpp
  src/application_poetry/ProviderGit.cpp src/application_poetry/ProviderContext.cpp
  src/application_poetry/ProviderPoetry.cpp)
target_link_libraries(batbox_application_poetry PUBLIC batbox_application_config
  batbox_poetryEntries batbox_context batbox_git_utils batbox_git batbox_instruction_async
  batbox_instructions batbox_glob batbox_process batbox_roost batbox_project_recovery)
target_compile_options(batbox_application_poetry PRIVATE -Wall -Wextra -Wpedantic)

add_library(batbox_application_presentation src/application_presentation/Bindings.cpp)
target_link_libraries(batbox_application_presentation PUBLIC batbox_conversation_shell
  batbox_text_editor batbox_npm_wrap_ansi)
target_compile_options(batbox_application_presentation PRIVATE -Wall -Wextra -Wpedantic)

include(cmake/IndigenousBats.cmake)
add_library(batbox_application_permissions INTERFACE)
target_link_libraries(batbox_application_permissions INTERFACE batbox_application_bats)

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
  batbox_application_poetry batbox_application_flights batbox_application_bats
  batbox_attachments batbox_bootstrap_state batbox_memory_prefetch)

target_sources(batbox_application_gadgets PRIVATE src/application_gadgets/DefaultInputs.cpp)
target_link_libraries(batbox_application_gadgets PUBLIC batbox_application_model
  batbox_application_connectors batbox_application_shell batbox_application_bats
  batbox_gadget_results batbox_settings)

add_library(batbox_application_frontend src/application_frontend/Defaults.cpp)
target_link_libraries(batbox_application_frontend PUBLIC batbox_application_runtime
  batbox_application_poetry batbox_application_flights batbox_application_bats
  batbox_message_runtime)
target_compile_options(batbox_application_frontend PRIVATE -Wall -Wextra -Wpedantic)

include(cmake/IndigenousCli.cmake)
include(cmake/IndigenousMemory.cmake)
include(cmake/IndigenousCommands.cmake)
include(cmake/IndigenousEndpointConfig.cmake)
include(cmake/IndigenousMissionUi.cmake)
include(cmake/MissionManagement.cmake)
include(cmake/IndigenousRestore.cmake)
include(cmake/IndigenousMedia.cmake)
include(cmake/IndigenousHistory.cmake)
include(cmake/IndigenousEditor.cmake)
include(cmake/IndigenousCompletion.cmake)
include(cmake/IndigenousFileHistory.cmake)
include(cmake/IndigenousShellShutdown.cmake)
include(cmake/IndigenousSkills.cmake)
include(cmake/IndigenousMcp.cmake)
include(cmake/IndigenousHost.cmake)
include(cmake/IndigenousElicitation.cmake)
