# openai-chat wire client: translation goldens, the fake Chat Completions
# server and a terminal smoke against it.
set(batbox_assertions
  $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
batbox_indigenous(phase6-openai-chat-translate-indigenous
  tests/phase6/openai_chat_translate_indigenous.cpp "batbox_transport;batbox_core")
target_compile_options(batbox-phase6-openai-chat-translate-indigenous PRIVATE ${batbox_assertions})
add_executable(batbox-openai-chat-http-probe tests/phase6/openai_chat_http_probe.cpp)
target_link_libraries(batbox-openai-chat-http-probe PRIVATE batbox_transport batbox_core)
add_test(NAME phase6-openai-chat-http COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_SOURCE_DIR}/tests/phase6/openai_chat_http.py --probe
  $<TARGET_FILE:batbox-openai-chat-http-probe>)
set_tests_properties(phase6-openai-chat-http PROPERTIES TIMEOUT 90)
if(UNIX)
  add_test(NAME phase6-openai-chat-smoke COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/checks/openai_chat_smoke.py $<TARGET_FILE:batbox>)
  set_tests_properties(phase6-openai-chat-smoke PROPERTIES TIMEOUT 70)
  add_test(NAME phase6-endpoint-betas-smoke COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/checks/endpoint_betas_smoke.py $<TARGET_FILE:batbox>)
  set_tests_properties(phase6-endpoint-betas-smoke PROPERTIES TIMEOUT 120)
  add_test(NAME phase6-endpoint-redirect-smoke COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/checks/endpoint_redirect_smoke.py $<TARGET_FILE:batbox>)
  set_tests_properties(phase6-endpoint-redirect-smoke PROPERTIES TIMEOUT 120)
endif()
