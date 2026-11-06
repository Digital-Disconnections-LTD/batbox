# Byte-identity and copy-count coverage for request assembly, serialization and stream consumption.
batbox_indigenous(phase6-json-stringify-identity-indigenous
  tests/phase6/json_stringify_identity_indigenous.cpp batbox_core)
batbox_indigenous(phase6-json-parse-identity-indigenous
  tests/phase6/json_parse_identity_indigenous.cpp batbox_core)
batbox_indigenous(phase6-context-conversation-sharing-indigenous
  tests/phase6/context_conversation_sharing_indigenous.cpp batbox_core)
batbox_indigenous(phase6-sse-chunking-identity-indigenous
  tests/phase6/sse_chunking_identity_indigenous.cpp batbox_transport)
batbox_indigenous(phase6-stream-event-batch-indigenous
  tests/phase6/stream_event_batch_indigenous.cpp batbox_core)
batbox_indigenous(phase6-stream-watchdog-resets-indigenous
  tests/phase6/stream_watchdog_resets_indigenous.cpp batbox_query)
batbox_indigenous(phase6-messages-body-identity-indigenous
  tests/phase6/messages_body_identity_indigenous.cpp batbox_transport)
