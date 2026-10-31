add_executable(batbox-core_commands-probe tests/phase5/core_commands_probe.cpp)
target_link_libraries(batbox-core_commands-probe PRIVATE batbox_core_commands)

batbox_indigenous(phase5-core-commands-indigenous tests/phase5/core_commands_indigenous.cpp batbox_core_commands)
