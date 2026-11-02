add_executable(batbox-phase2-cloud-sigv4 tests/phase2/cloud_sigv4_indigenous.cpp)
target_link_libraries(batbox-phase2-cloud-sigv4 PRIVATE batbox_cloud_providers)
add_test(NAME phase2-cloud-sigv4 COMMAND batbox-phase2-cloud-sigv4 ${CMAKE_SOURCE_DIR}/tests/fixtures/sigv4)
batbox_indigenous(phase2-cloud-request-shaping tests/phase2/cloud_request_shaping_indigenous.cpp batbox_cloud_providers)
add_executable(batbox-cloud-providers-probe tests/phase2/cloud_providers_probe.cpp)
target_link_libraries(batbox-cloud-providers-probe PRIVATE batbox_cloud_providers)
add_test(NAME phase2-cloud-eventstream COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_SOURCE_DIR}/tests/phase2/cloud_eventstream.py --probe $<TARGET_FILE:batbox-cloud-providers-probe>)
if(NOT WIN32)
  batbox_indigenous(phase2-cloud-credentials tests/phase2/cloud_credentials_indigenous.cpp batbox_cloud_providers)
  add_test(NAME phase2-cloud-providers COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/phase2/cloud_providers.py --probe $<TARGET_FILE:batbox-cloud-providers-probe>)
endif()
