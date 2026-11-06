batbox_indigenous(phase4-settings-indigenous-schema tests/phase4/settings_indigenous_schema.cpp batbox_settings)
batbox_indigenous(phase5-git-basics-indigenous tests/phase5/git_basics_indigenous.cpp batbox_git_utils)
batbox_indigenous(phase5-instructions-async-indigenous tests/phase5/instructions_async_indigenous.cpp batbox_instruction_async)

batbox_indigenous(phase5-instructions-loaded-indigenous tests/phase5/instructions_loaded_indigenous.cpp batbox_instruction_async)
batbox_indigenous(phase5-cwd-cleanup-indigenous tests/phase5/cwd_cleanup_indigenous.cpp batbox_roost)
batbox_indigenous(phase5-shell-bootstrap-indigenous tests/phase5/shell_bootstrap_indigenous.cpp batbox_roost)
batbox_indigenous(phase5-cleanup-retention-indigenous tests/phase5/cleanup_retention_indigenous.cpp batbox_cleanup)
batbox_indigenous(phase5-cleanup-extra-indigenous tests/phase5/cleanup_extra_indigenous.cpp batbox_cleanup)
batbox_indigenous(phase5-cleanup-terminal-indigenous tests/phase5/cleanup_terminal_indigenous.cpp batbox_cleanup)

batbox_indigenous(phase4-settings-indigenous-watcher tests/phase4/settings_indigenous_watcher_probe.cpp batbox_settings)
batbox_indigenous(phase5-glob-binding-indigenous tests/phase5/glob_binding_indigenous.cpp batbox_glob)


batbox_indigenous(phase4-bat-result-bindings-indigenous tests/phase4/bat_result_bindings_indigenous.cpp batbox_bats)

batbox_indigenous(phase5-flight-title-bound-indigenous
  tests/phase5/flight_title_bound_indigenous.cpp batbox_flight_titles)

batbox_indigenous(phase4-simple-collect-indigenous tests/phase4/simple_collect_indigenous.cpp batbox_query)

batbox_indigenous(phase5-hooks-http-indigenous tests/phase5/hooks_http_indigenous.cpp batbox_instruction_async)
batbox_indigenous(phase5-hooks-async-indigenous tests/phase5/hooks_async_indigenous.cpp batbox_instruction_async)
batbox_indigenous(phase5-hooks-command-full-indigenous tests/phase5/hooks_command_full_indigenous.cpp batbox_instruction_async)
batbox_indigenous(phase4-stream-watchdog-owner-indigenous tests/phase4/stream_watchdog_owner_indigenous.cpp batbox_query)

include(cmake/HookRuntimeTests.cmake)
include(cmake/AuthRuntimeTests.cmake)
include(cmake/FilesystemRuntimeTests.cmake)
include(cmake/HistoryRuntimeTests.cmake)
include(cmake/ClientFactoryTests.cmake)
include(cmake/MonsterTests.cmake)
include(cmake/IntlTests.cmake)
include(cmake/TextWidthTests.cmake)
include(cmake/SyntheticOutputTests.cmake)
include(cmake/BootstrapTests.cmake)
include(cmake/AppStateTests.cmake)
include(cmake/CronTests.cmake)
include(cmake/TerminalEventsTests.cmake)
include(cmake/KeyboardInputTests.cmake)
include(cmake/ClipboardTests.cmake)
include(cmake/TruncateTests.cmake)

include(cmake/AsyncContextTests.cmake)
include(cmake/BatContextTests.cmake)
include(cmake/CronIntegrationTests.cmake)
include(cmake/HeadlessCliTests.cmake)
include(cmake/StructuredRootTests.cmake)
include(cmake/ProtocolHelpersTests.cmake)
include(cmake/ConversationRuntimeTests.cmake)
include(cmake/StructuredFlightTests.cmake)
include(cmake/FtxuiPresentationTests.cmake)
include(cmake/UserInputTests.cmake)
