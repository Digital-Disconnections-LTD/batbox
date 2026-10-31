add_executable(batbox-async_context-probe tests/phase3/async_context_probe.cpp)
target_link_libraries(batbox-async_context-probe PRIVATE batbox_event_loop)

batbox_indigenous(phase3-async-context-indigenous tests/phase3/async_context_indigenous.cpp batbox_event_loop)
batbox_indigenous(phase3-async-launch-indigenous tests/phase3/async_launch_indigenous.cpp batbox_roost)
batbox_probe(cwd_async batbox_roost phase5)
batbox_probe(value_truthy batbox_core phase3)
