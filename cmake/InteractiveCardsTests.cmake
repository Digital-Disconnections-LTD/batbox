batbox_indigenous(phase6-interactive-collapsed-indigenous
  tests/phase6/interactive_collapsed_projection_indigenous.cpp batbox_terminal_ftxui)
batbox_indigenous(phase6-interactive-gadget-child-binding-indigenous
  tests/phase6/interactive_gadget_child_binding_indigenous.cpp batbox_terminal_ftxui)
batbox_indigenous(phase6-interactive-remaining-cards-indigenous
  tests/phase6/interactive_remaining_cards_indigenous.cpp batbox_terminal_ftxui)

foreach(section IN ITEMS hook_progress gadget_error gadget_result gadget_success gadget_reject assistant_gadget_use)
  batbox_indigenous(phase6-interactive-${section}-projection-indigenous
    tests/phase6/interactive_${section}_projection_indigenous.cpp batbox_terminal_ftxui)
endforeach()
foreach(section IN ITEMS assistant_projection assistant_binding
    system_api_error system_text_projection)
  batbox_indigenous(phase6-interactive-${section}-indigenous
    tests/phase6/interactive_${section}_indigenous.cpp batbox_terminal_ftxui)
endforeach()
foreach(section IN ITEMS user_card_binding user_text_projection user_poetry_projection
    output_line_projection bash_result_projection)
  batbox_indigenous(phase6-interactive-${section}-indigenous
    tests/phase6/interactive_${section}_indigenous.cpp batbox_terminal_ftxui)
endforeach()
