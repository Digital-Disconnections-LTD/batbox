batbox_indigenous(phase4-context-async-indigenous tests/phase4/context_async_indigenous.cpp "batbox_context;batbox_process")

batbox_indigenous(phase4-system-poetry-head-indigenous
  tests/phase4/system_poetry_head_indigenous.cpp batbox_poetryEntries)
target_compile_definitions(batbox-phase4-system-poetry-head-indigenous PRIVATE
  BATBOX_SYSTEM_POETRY_FILE="${CMAKE_SOURCE_DIR}/poetry/system.md")
batbox_indigenous(phase3-exec-file-indigenous tests/phase3/exec_file_indigenous.cpp batbox_process)
batbox_indigenous(phase3-git-fs-indigenous tests/phase3/git_fs_indigenous.cpp batbox_git)

batbox_indigenous(phase3-bash-async-persistence-indigenous tests/phase3/bash_async_persistence_indigenous.cpp batbox_shell)
batbox_indigenous(phase3-bash-async-image-indigenous tests/phase3/bash_async_image_indigenous.cpp batbox_shell_media)
batbox_indigenous(phase3-bash-async-fixture-indigenous tests/phase3/bash_async_fixture_indigenous.cpp batbox_shell)
batbox_indigenous(phase4-query-owner-retry-stage tests/phase4/query_owner_retry_stage_indigenous.cpp batbox_query)
batbox_indigenous(phase4-query-owner-attempt-stage tests/phase4/query_owner_attempt_stage_indigenous.cpp batbox_query)

batbox_indigenous(phase4-query-owner-promise-all tests/phase4/query_owner_promise_all_indigenous.cpp batbox_query)
batbox_indigenous(phase4-query-owner-stream-close tests/phase4/query_owner_stream_close_indigenous.cpp batbox_query)

batbox_indigenous(phase4-query-owner-model-segments tests/phase4/query_owner_model_segments_indigenous.cpp batbox_query)
foreach(section IN ITEMS query_async_model_lifetime query_indigenous_model_cursor query_indigenous_gadget_cursor
    query_indigenous_blocked_producer query_async_operation_queue query_closeable_indigenous_cursor)
  batbox_indigenous(phase4-${section} tests/phase4/${section}_indigenous.cpp batbox_query)
endforeach()

foreach(section IN ITEMS query_indigenous_blocked_http query_indigenous_close_http)
  add_executable(batbox-${section}-probe tests/phase4/${section}_probe.cpp)
  target_link_libraries(batbox-${section}-probe PRIVATE batbox_query)
  add_test(NAME phase4-${section} COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/phase4/${section}.py --probe $<TARGET_FILE:batbox-${section}-probe>)
endforeach()
add_executable(batbox-query-async-e2e tests/phase4/query_async_e2e_probe.cpp
  tests/phase4/query_async_e2e_model_fixture.cpp)
target_link_libraries(batbox-query-async-e2e PRIVATE batbox_query batbox_gadget_results batbox_shell)
foreach(gadget IN ITEMS Read Bash)
  add_test(NAME phase4-query-async-e2e-${gadget} COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/phase4/query_async_e2e_http.py
    --probe $<TARGET_FILE:batbox-query-async-e2e> --gadget ${gadget})
endforeach()

target_sources(batbox-query_indigenous_blocked_http-probe PRIVATE
  tests/phase4/query_async_e2e_model_fixture.cpp)
target_sources(batbox-query_indigenous_close_http-probe PRIVATE
  tests/phase4/query_async_e2e_model_fixture.cpp)
