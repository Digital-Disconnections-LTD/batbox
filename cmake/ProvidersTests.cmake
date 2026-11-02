add_executable(batbox-providers-catalog-indigenous tests/phase6/providers_catalog_indigenous.cpp)
target_link_libraries(batbox-providers-catalog-indigenous PRIVATE batbox_providers)
add_test(NAME phase6-providers-catalog-indigenous COMMAND batbox-providers-catalog-indigenous)
add_test(NAME phase6-providers-catalog-generator COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_SOURCE_DIR}/checks/providers_catalog_generator.py ${CMAKE_SOURCE_DIR})
add_executable(batbox-providers-discovery-indigenous tests/phase6/providers_discovery_indigenous.cpp)
target_link_libraries(batbox-providers-discovery-indigenous PRIVATE batbox_providers)
add_test(NAME phase6-providers-discovery-indigenous COMMAND ${Python3_EXECUTABLE}
  ${CMAKE_SOURCE_DIR}/tests/phase6/providers_fake_servers.py
  --probe $<TARGET_FILE:batbox-providers-discovery-indigenous>)
set_tests_properties(phase6-providers-discovery-indigenous PROPERTIES TIMEOUT 60)
if(UNIX)
  add_test(NAME phase6-providers-cli COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/checks/providers_cli.py $<TARGET_FILE:batbox>)
  set_tests_properties(phase6-providers-cli PROPERTIES TIMEOUT 90)
  add_test(NAME phase6-providers-first-run-smoke COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/checks/providers_first_run_smoke.py $<TARGET_FILE:batbox>)
  set_tests_properties(phase6-providers-first-run-smoke PROPERTIES TIMEOUT 90)
endif()
