foreach(section IN ITEMS websocket_transport sse_frames sse_transport serial_batch
    hybrid_transport worker_state stream_accumulator ccr_runtime flight_token websocket_proxy)
  add_executable(batbox-${section}-probe tests/phase5/${section}_probe.cpp)
  target_link_libraries(batbox-${section}-probe PRIVATE batbox_remote)

endforeach()
batbox_indigenous(phase5-websocket-indigenous tests/phase5/websocket_indigenous_local.cpp batbox_remote)
batbox_indigenous(phase5-sse-indigenous tests/phase5/sse_indigenous_local.cpp batbox_remote)
batbox_indigenous(phase5-ccr-http-indigenous tests/phase5/ccr_http_indigenous_local.cpp batbox_remote)
batbox_indigenous(phase5-remote-binding-indigenous tests/phase5/remote_binding_indigenous.cpp batbox_remote)
