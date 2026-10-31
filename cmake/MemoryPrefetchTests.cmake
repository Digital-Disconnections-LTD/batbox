foreach(section IN ITEMS poetry selector start path header)
  add_executable(batbox-memory_prefetch_${section}-probe
    tests/phase5/memory_prefetch_${section}_probe.cpp)
  target_link_libraries(batbox-memory_prefetch_${section}-probe PRIVATE batbox_memory_prefetch)

endforeach()
batbox_indigenous(phase5-memory-prefetch-indigenous tests/phase5/memory_prefetch_indigenous.cpp
  batbox_memory_prefetch)
