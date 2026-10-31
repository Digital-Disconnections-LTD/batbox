target_sources(batbox_transport PRIVATE src/transport/SdkRetry.cpp
  src/transport/SdkRetryClock.cpp src/transport/AsyncMessages.cpp)
target_link_libraries(batbox_transport PUBLIC batbox_event_loop)
add_library(batbox_side_query src/side_query/SideQuery.cpp
  src/side_query/IndigenousBinding.cpp)
target_link_libraries(batbox_side_query PUBLIC batbox_client_factory batbox_transport)
target_compile_options(batbox_side_query PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_memory_prefetch PRIVATE src/memory_prefetch/Bindings.cpp)
target_link_libraries(batbox_memory_prefetch PUBLIC batbox_side_query)
