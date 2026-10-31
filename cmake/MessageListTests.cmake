foreach(section IN ITEMS interactive_message_order interactive_message_visibility
    interactive_message_decisions interactive_message_grouping
    interactive_message_collapse interactive_message_background
    interactive_message_lookups interactive_gadget_classification
    interactive_message_synthetic interactive_read_search)
  batbox_probe(${section} batbox_interactive phase6)
endforeach()
batbox_indigenous(phase6-interactive-message-pipeline-indigenous
  tests/phase6/interactive_message_pipeline_indigenous.cpp batbox_interactive)
