foreach(section IN ITEMS interactive_user_text_card interactive_user_text_leaves
    interactive_user_poetry interactive_thinking_highlight
    interactive_bash_result_text interactive_bash_result_card interactive_output_line)
  batbox_probe(${section} batbox_interactive phase6)
endforeach()
foreach(section IN ITEMS interactive_collapsed interactive_verbose_gadget_use)
  batbox_probe(${section} batbox_interactive phase6)
endforeach()
batbox_indigenous(phase6-interactive-collapsed-indigenous
  tests/phase6/interactive_collapsed_projection_indigenous.cpp batbox_terminal_ftxui)
batbox_indigenous(phase6-interactive-gadget-child-binding-indigenous
  tests/phase6/interactive_gadget_child_binding_indigenous.cpp batbox_terminal_ftxui)
batbox_probe(interactive_remaining_cards batbox_interactive phase6)
batbox_indigenous(phase6-interactive-remaining-cards-indigenous
  tests/phase6/interactive_remaining_cards_indigenous.cpp batbox_terminal_ftxui)
add_executable(batbox-interactive-min-hint-indigenous tests/phase6/interactive_min_hint_indigenous.cpp)
target_link_libraries(batbox-interactive-min-hint-indigenous PRIVATE batbox_interactive)

foreach(section IN ITEMS interactive_fallback_error interactive_hook_progress
    interactive_gadget_error interactive_gadget_result interactive_gadget_success
    interactive_gadget_reject interactive_assistant_gadget_use interactive_grouped_gadget_use)
  batbox_probe(${section} batbox_interactive phase6)
endforeach()
foreach(section IN ITEMS hook_progress gadget_error gadget_result gadget_success gadget_reject assistant_gadget_use)
  batbox_indigenous(phase6-interactive-${section}-projection-indigenous
    tests/phase6/interactive_${section}_projection_indigenous.cpp batbox_terminal_ftxui)
endforeach()
foreach(section IN ITEMS interactive_assistant_text interactive_assistant_thinking
    interactive_rate_limit interactive_system_api_error interactive_system_text)
  batbox_probe(${section} batbox_interactive phase6)
endforeach()
foreach(section IN ITEMS assistant_projection assistant_binding rate_limit
    system_api_error system_text_projection)
  batbox_indigenous(phase6-interactive-${section}-indigenous
    tests/phase6/interactive_${section}_indigenous.cpp batbox_terminal_ftxui)
endforeach()
foreach(section IN ITEMS user_card_binding user_text_projection user_poetry_projection
    output_line_projection bash_result_projection)
  batbox_indigenous(phase6-interactive-${section}-indigenous
    tests/phase6/interactive_${section}_indigenous.cpp batbox_terminal_ftxui)
endforeach()
