# The hook runtime: all of src/instructions/hooks/runtime except
# HookResponseSchema.cpp, which stays its own leaf target (hook_schema in
# cmake/SyntheticOutput.cmake) so schema consumers can reach it without the
# runtime. The 60 runtime sources were compiled in pieces inside
# batbox_instruction_async and the host-runtime slice; the hook execution
# system is one module, reached through instruction_async's PUBLIC link.
if(WIN32)
  set(BATBOX_HOOK_COMMAND_SOURCE src/instructions/hooks/runtime/IndigenousCommandProcessWin.cpp)
  set(BATBOX_HOOK_MANAGED_SOURCE src/instructions/hooks/runtime/IndigenousManagedHookProcessWin.cpp)
else()
  set(BATBOX_HOOK_COMMAND_SOURCE src/instructions/hooks/runtime/IndigenousCommandProcess.cpp)
  set(BATBOX_HOOK_MANAGED_SOURCE src/instructions/hooks/runtime/IndigenousManagedHookProcess.cpp)
endif()
add_library(batbox_hooks
  src/instructions/hooks/runtime/AsyncHookDetection.cpp
  src/instructions/hooks/runtime/AsyncHookRoster.cpp
  src/instructions/hooks/runtime/AsyncHookRosterFinalize.cpp
  src/instructions/hooks/runtime/AsyncHookRosterPoll.cpp
  src/instructions/hooks/runtime/BackgroundHook.cpp
  src/instructions/hooks/runtime/BaseHookInput.cpp
  src/instructions/hooks/runtime/BatHookExecutor.cpp
  src/instructions/hooks/runtime/BatOwnerQueryBinding.cpp
  src/instructions/hooks/runtime/BatSyntheticGadgetBinding.cpp
  src/instructions/hooks/runtime/CombinedAbort.cpp
  src/instructions/hooks/runtime/CompactHooks.cpp
  src/instructions/hooks/runtime/ElicitationHookParser.cpp
  src/instructions/hooks/runtime/ElicitationHooks.cpp
  src/instructions/hooks/runtime/EnvHooks.cpp
  src/instructions/hooks/runtime/ExecuteHooks.cpp
  src/instructions/hooks/runtime/FlightEnvironment.cpp
  src/instructions/hooks/runtime/FlightEnvironmentClear.cpp
  src/instructions/hooks/runtime/FlightEnvironmentFs.cpp
  src/instructions/hooks/runtime/FlightEnvironmentLoad.cpp
  src/instructions/hooks/runtime/FlightHookBinding.cpp
  src/instructions/hooks/runtime/FlightHookStore.cpp
  src/instructions/hooks/runtime/FlightHookViews.cpp
  src/instructions/hooks/runtime/FunctionHook.cpp
  src/instructions/hooks/runtime/HookAbort.cpp
  src/instructions/hooks/runtime/HookBatchStart.cpp
  src/instructions/hooks/runtime/HookBlockingMessages.cpp
  src/instructions/hooks/runtime/HookCallback.cpp
  src/instructions/hooks/runtime/HookConfig.cpp
  src/instructions/hooks/runtime/HookEventWrappers.cpp
  src/instructions/hooks/runtime/HookMatching.cpp
  src/instructions/hooks/runtime/HookOutputParser.cpp
  src/instructions/hooks/runtime/HookPerHookCommand.cpp
  src/instructions/hooks/runtime/HookPerHookHttp.cpp
  src/instructions/hooks/runtime/HookPerHookModel.cpp
  src/instructions/hooks/runtime/HookPerHookSimple.cpp
  src/instructions/hooks/runtime/HookPoetryStream.cpp
  src/instructions/hooks/runtime/HookResultLoop.cpp
  src/instructions/hooks/runtime/HookRuntimeBindings.cpp
  src/instructions/hooks/runtime/HookSchema.cpp
  src/instructions/hooks/runtime/HttpHook.cpp
  src/instructions/hooks/runtime/IfCondition.cpp
  src/instructions/hooks/runtime/IndigenousCommandFull.cpp
  src/instructions/hooks/runtime/IndigenousCommandPreparation.cpp
  src/instructions/hooks/runtime/IndigenousCommandPrepared.cpp
  ${BATBOX_HOOK_COMMAND_SOURCE}
  src/instructions/hooks/runtime/IndigenousHttp.cpp
  ${BATBOX_HOOK_MANAGED_SOURCE}
  src/instructions/hooks/runtime/LazyHookJsonInput.cpp
  src/instructions/hooks/runtime/NotificationHooks.cpp
  src/instructions/hooks/runtime/OutsideRepl.cpp
  src/instructions/hooks/runtime/OutsideReplBranches.cpp
  src/instructions/hooks/runtime/PoetryHookExecutor.cpp
  src/instructions/hooks/runtime/PoetryModelBinding.cpp
  src/instructions/hooks/runtime/ProcessHookJson.cpp
  src/instructions/hooks/runtime/ProcessHookJsonCommon.cpp
  src/instructions/hooks/runtime/ProcessHookJsonSpecific.cpp
  src/instructions/hooks/runtime/SsrfGuard.cpp
  src/instructions/hooks/runtime/StructuredOutputEnforcement.cpp
  src/instructions/hooks/runtime/WorktreeHooks.cpp)
target_link_libraries(batbox_hooks PUBLIC batbox_core batbox_event_loop
  batbox_instructions batbox_context batbox_glob batbox_shell batbox_settings
  batbox_query batbox_synthetic_output batbox_hook_schema CURL::libcurl)
target_compile_options(batbox_hooks PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(batbox_instruction_async PUBLIC batbox_hooks)
