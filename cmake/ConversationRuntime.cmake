add_library(batbox_conversation
  src/query_engine/Lifecycle.cpp src/query_engine/LifecyclePrepare.cpp
  src/query_engine/LifecycleContext.cpp src/query_engine/LifecycleLocal.cpp
  src/query_engine/LifecycleStream.cpp src/query_engine/LifecycleResult.cpp
  src/query_engine/Ask.cpp src/query_engine/ResultHelpers.cpp)
target_link_libraries(batbox_conversation PUBLIC batbox_query_engine_state batbox_query)
target_compile_options(batbox_conversation PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_react_sdk
  src/react_sdk/Value.cpp src/react_sdk/Props.cpp src/react_sdk/Element.cpp
  src/react_sdk/Context.cpp src/react_sdk/Components.cpp src/react_sdk/Hooks.cpp
  src/react_sdk/Children.cpp src/react_sdk/Lazy.cpp)
target_link_libraries(batbox_react_sdk PUBLIC batbox_core)
target_compile_options(batbox_react_sdk PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_react_host src/react_sdk/HostBridge.cpp)
target_link_libraries(batbox_react_host PUBLIC batbox_react_sdk batbox_terminal_reconciler)
add_library(batbox_react_scheduler src/react_scheduler/Heap.cpp
  src/react_scheduler/Scheduler.cpp src/react_scheduler/IndigenousHost.cpp)
target_link_libraries(batbox_react_scheduler PUBLIC batbox_event_loop)
target_compile_options(batbox_react_scheduler PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_react_fiber src/react_fiber/Fiber.cpp src/react_fiber/Root.cpp)
target_link_libraries(batbox_react_fiber PUBLIC batbox_react_sdk)
target_compile_options(batbox_react_fiber PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_lodash_throttle src/lodash_throttle/Throttle.cpp src/lodash_throttle/ThrottleIndigenous.cpp)
target_link_libraries(batbox_lodash_throttle PUBLIC batbox_event_loop)
target_compile_options(batbox_lodash_throttle PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_terminal_runtime PRIVATE
  src/terminal_runtime/TerminalPatchConsole.cpp src/terminal_runtime/TerminalPatchStderr.cpp
  src/terminal_runtime/TerminalScheduler.cpp src/terminal_runtime/TerminalProcessLifecycle.cpp
  src/terminal_runtime/TerminalReactTree.cpp src/terminal_runtime/TerminalStartup.cpp)
target_link_libraries(batbox_terminal_runtime PUBLIC batbox_react_sdk batbox_lodash_throttle)
