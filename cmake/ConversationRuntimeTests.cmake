batbox_probe(query_engine_lifecycle batbox_conversation phase5)
foreach(section IN ITEMS element hooks children lazy)
  batbox_probe(react_sdk_${section} batbox_react_sdk phase6)
endforeach()
batbox_probe(react_scheduler batbox_react_scheduler phase6)
batbox_probe(react_fiber_root batbox_react_fiber phase6)
batbox_indigenous(phase6-react-host-bridge-indigenous tests/phase6/react_sdk_host_bridge.cpp batbox_react_host)
batbox_indigenous(phase6-react-scheduler-indigenous tests/phase6/react_scheduler_indigenous.cpp batbox_react_scheduler)
batbox_probe(indigenous_lodash_throttle batbox_lodash_throttle phase5)
target_sources(batbox-indigenous_ink_controls-probe PRIVATE
  src/terminal_runtime/TerminalPatchConsole.cpp src/terminal_runtime/TerminalPatchStderr.cpp
  src/terminal_runtime/TerminalScheduler.cpp src/terminal_runtime/TerminalProcessLifecycle.cpp
  src/terminal_runtime/TerminalReactTree.cpp src/terminal_runtime/TerminalStartup.cpp
  src/terminal_events/Event.cpp src/terminal_events/KeyboardEvent.cpp
  src/terminal_events/Dispatcher.cpp src/terminal_events/HandlerMap.cpp)
target_link_libraries(batbox-indigenous_ink_controls-probe PRIVATE batbox_react_sdk batbox_lodash_throttle)
