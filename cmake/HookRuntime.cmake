target_sources(batbox_instruction_async PRIVATE
  src/instructions/hooks/runtime/HookPolicyLeaves.cpp
  src/instructions/hooks/runtime/FlightEnvironment.cpp
  src/instructions/hooks/runtime/FlightEnvironmentClear.cpp
  src/instructions/hooks/runtime/FlightEnvironmentLoad.cpp
  src/instructions/hooks/runtime/FlightEnvironmentFs.cpp
  src/instructions/hooks/runtime/EnvHooks.cpp
  src/instructions/hooks/runtime/ConfigChangeHooks.cpp
  src/instructions/hooks/runtime/NotificationHooks.cpp
  src/instructions/hooks/runtime/CompactHooks.cpp
  src/instructions/hooks/runtime/FlightEndHooks.cpp
  src/instructions/hooks/runtime/HookRuntimeBindings.cpp
  src/instructions/hooks/runtime/HookBlockingMessages.cpp
  src/instructions/hooks/runtime/StopFailureHooks.cpp
  src/instructions/hooks/runtime/WorktreeHooks.cpp
  src/instructions/hooks/runtime/ElicitationHookParser.cpp
  src/instructions/hooks/runtime/ElicitationHooks.cpp
  src/instructions/hooks/runtime/CombinedAbort.cpp
  src/instructions/hooks/runtime/FunctionHook.cpp
  src/instructions/hooks/runtime/ProcessHookJson.cpp
  src/instructions/hooks/runtime/ProcessHookJsonCommon.cpp
  src/instructions/hooks/runtime/ProcessHookJsonSpecific.cpp
  src/instructions/hooks/runtime/HookCallback.cpp
  src/instructions/hooks/runtime/CommandLeaves.cpp
)
target_sources(batbox_instruction_async PRIVATE
  src/instructions/hooks/runtime/HookBatchStart.cpp
  src/instructions/hooks/runtime/HookResultLoop.cpp
  src/instructions/hooks/runtime/LazyHookJsonInput.cpp
  src/instructions/hooks/runtime/HookPerHookSimple.cpp
  src/instructions/hooks/runtime/HookPerHookCommand.cpp
  src/instructions/hooks/runtime/HookPerHookModel.cpp
  src/instructions/hooks/runtime/HookPerHookHttp.cpp
  src/instructions/hooks/runtime/ExecuteHooks.cpp)
target_sources(batbox_instruction_async PRIVATE src/instructions/hooks/runtime/PoetryHookExecutor.cpp)
target_sources(batbox_instruction_async PRIVATE src/instructions/hooks/runtime/BatHookExecutor.cpp)
target_sources(batbox_instruction_async PRIVATE
  src/instructions/hooks/runtime/HookModelBindings.cpp
  src/instructions/hooks/runtime/PoetryModelBinding.cpp
  src/instructions/hooks/runtime/BatOwnerQueryBinding.cpp
)
target_link_libraries(batbox_instruction_async PUBLIC batbox_query)
target_sources(batbox_instruction_async PRIVATE
  src/instructions/hooks/runtime/StructuredOutputEnforcement.cpp
  src/instructions/hooks/runtime/FlightHookStore.cpp
  src/instructions/hooks/runtime/FlightHookViews.cpp
  src/instructions/hooks/runtime/FlightHookBinding.cpp)
target_sources(batbox_instruction_async PRIVATE src/instructions/hooks/runtime/BatSyntheticGadgetBinding.cpp)
target_link_libraries(batbox_instruction_async PUBLIC batbox_synthetic_output batbox_hook_schema)
target_sources(batbox_instruction_async PRIVATE
  src/instructions/hooks/runtime/HookEventWrappers.cpp src/instructions/hooks/runtime/HookEventWrapperBinding.cpp
  src/instructions/hooks/runtime/SimpleEventWrappers.cpp
  src/instructions/hooks/runtime/GatedGadgetWrappers.cpp src/instructions/hooks/runtime/GatedGadgetWrapperBinding.cpp
  src/instructions/hooks/runtime/PoetryGadgetWrappers.cpp src/instructions/hooks/runtime/PoetryGadgetWrapperBinding.cpp
  src/instructions/hooks/runtime/StopPoetryWrappers.cpp src/instructions/hooks/runtime/StopPoetryWrapperBinding.cpp
  src/instructions/hooks/runtime/HookSourceLeaves.cpp)
