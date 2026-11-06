batbox_indigenous(phase5-memory-prefetch-indigenous tests/phase5/memory_prefetch_indigenous.cpp
  batbox_memory_prefetch)
add_executable(batbox-phase5-memory-recall-selection-indigenous
  tests/phase5/memory_recall_selection_indigenous.cpp)
target_link_libraries(batbox-phase5-memory-recall-selection-indigenous PRIVATE
  batbox_memory_prefetch batbox_application_memory)
add_test(NAME phase5-memory-recall-selection-indigenous
  COMMAND batbox-phase5-memory-recall-selection-indigenous)
