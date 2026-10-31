add_library(batbox_memory_prefetch
  src/memory_prefetch/PoetryScan.cpp src/memory_prefetch/BatMemoryPath.cpp
  src/memory_prefetch/IndigenousIO.cpp src/memory_prefetch/Read.cpp
  src/memory_prefetch/Selector.cpp src/memory_prefetch/Collect.cpp
  src/memory_prefetch/Start.cpp)
target_link_libraries(batbox_memory_prefetch PUBLIC batbox_query batbox_instructions)
target_compile_options(batbox_memory_prefetch PRIVATE -Wall -Wextra -Wpedantic)
