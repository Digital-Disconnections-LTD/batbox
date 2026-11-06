# Indigenous bindings for durable project memory, recall and maintenance.
add_library(batbox_application_memory
  src/application_memory/Provider.cpp src/application_memory/Sources.cpp
  src/application_memory/Recall.cpp src/application_memory/Consolidation.cpp
  src/application_memory/ConsolidationLock.cpp
  src/application_memory/ConsolidationPoetry.cpp src/application_memory/Fork.cpp)
target_link_libraries(batbox_application_memory PUBLIC
  batbox_application_poetry batbox_application_model batbox_application_flights
  batbox_application_engine batbox_application_bats batbox_memory_prefetch
  batbox_side_query)
target_compile_options(batbox_application_memory PRIVATE -Wall -Wextra -Wpedantic)

if(BUILD_TESTING)
  add_executable(batbox-indigenous-memory-workflow tests/phase6/indigenous_memory_workflow.cpp)
  target_link_libraries(batbox-indigenous-memory-workflow PRIVATE batbox_application_memory)
  add_test(NAME phase6-indigenous-memory-workflow COMMAND batbox-indigenous-memory-workflow)
endif()
