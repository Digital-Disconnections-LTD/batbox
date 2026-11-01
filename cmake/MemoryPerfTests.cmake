# Regression coverage for per-frame copy, streaming, and resident-memory work.
batbox_indigenous(phase6-stream-accumulator-indigenous
  tests/phase6/stream_accumulator_indigenous.cpp batbox_interactive)
batbox_indigenous(phase6-markdown-stash-bounds-indigenous
  tests/phase6/markdown_stash_bounds_indigenous.cpp batbox_terminal_ftxui)
batbox_indigenous(phase6-message-list-memo-indigenous
  tests/phase6/message_list_memo_indigenous.cpp batbox_interactive)
batbox_indigenous(phase4-stash-safe-snapshot-indigenous
  tests/phase4/stash_safe_snapshot_indigenous.cpp batbox_bats)
batbox_indigenous(phase6-string-metrics-ascii-indigenous
  tests/phase6/string_metrics_ascii_indigenous.cpp batbox_terminal_ftxui)
batbox_indigenous(phase6-transcript-rows-incremental-indigenous
  tests/phase6/transcript_rows_incremental_indigenous.cpp batbox_terminal_ftxui)
batbox_indigenous(phase5-flights-load-memory-indigenous
  tests/phase5/flights_load_memory_indigenous.cpp batbox_flights)
batbox_indigenous(phase5-pump-schedule-indigenous
  tests/phase5/pump_schedule_indigenous.cpp batbox_terminal_ftxui)
if(UNIX)
  batbox_indigenous(phase5-ftxui-pump-backoff-pty
    tests/phase5/ftxui_pump_backoff_pty.cpp batbox_terminal_ftxui)
  if(NOT APPLE)
    target_link_libraries(batbox-phase5-ftxui-pump-backoff-pty PRIVATE util)
  endif()
endif()
batbox_indigenous(phase4-memory-usage-indigenous
  tests/phase4/memory_usage_indigenous.cpp batbox_profiler)
