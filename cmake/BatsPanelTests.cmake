# Bats list, footer, saved Bats and Bat record hygiene.
batbox_indigenous(phase4-bats-panel-state tests/phase4/bats_panel_state_indigenous.cpp batbox_bats)
batbox_indigenous(phase6-bats-list-indigenous
  tests/phase6/bats_list_indigenous.cpp batbox_application_commands)
batbox_indigenous(phase6-bats-saved-host
  tests/phase6/bats_saved_host_indigenous.cpp batbox_indigenous_host)
set_tests_properties(phase6-bats-saved-host PROPERTIES TIMEOUT 45)
batbox_indigenous(phase6-bat-conversation-view
  tests/phase6/bat_conversation_view_indigenous.cpp batbox_conversation_shell)
