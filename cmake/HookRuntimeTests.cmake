foreach(section IN ITEMS hooks_policy hooks_flight_env hooks_env hooks_config_change
    hooks_notification hooks_compact hooks_flight_end hooks_blocking_messages
    hooks_stop_failure hooks_worktree hooks_elicitation hooks_combined_abort
    hooks_function hooks_process_json hooks_callback hooks_command_leaves hooks_concurrent_all)
  batbox_probe(${section} batbox_instruction_async phase5)
endforeach()
batbox_indigenous(phase5-hooks-flight-env-indigenous tests/phase5/hooks_flight_env_indigenous.cpp batbox_instruction_async)
foreach(section IN ITEMS hooks_batch_start hooks_result_loop hooks_per_hook_simple
    hooks_per_hook_command hooks_per_hook_model hooks_per_hook_http hooks_execute)
  batbox_probe(${section} batbox_instruction_async phase5)
endforeach()
batbox_probe(hooks_execute_control batbox_instruction_async phase5)
batbox_probe(hooks_poetry_executor batbox_instruction_async phase5)
batbox_probe(hooks_bat_executor batbox_instruction_async phase5)
batbox_probe(hooks_response_schema batbox_instruction_async phase5)
batbox_indigenous(phase5-hooks-poetry-model-binding-indigenous tests/phase5/hooks_poetry_model_binding_indigenous.cpp batbox_instruction_async)
batbox_indigenous(phase5-hooks-bat-owner-query-indigenous tests/phase5/hooks_bat_owner_query_indigenous.cpp batbox_instruction_async)
foreach(section IN ITEMS hooks_structured_enforcement hooks_flight_store)
  batbox_probe(${section} batbox_instruction_async phase5)
endforeach()
batbox_indigenous(phase5-hooks-flight-binding-indigenous tests/phase5/hooks_flight_binding_indigenous.cpp batbox_instruction_async)
foreach(section IN ITEMS hooks_event_wrapper hooks_simple_event hooks_gated_gadget
    hooks_poetry_gadget hooks_stop_poetry hooks_source_leaves)
  batbox_probe(${section} batbox_instruction_async phase5)
endforeach()
