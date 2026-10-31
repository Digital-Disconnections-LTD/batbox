foreach(section IN ITEMS app_state_store app_state_default app_state_selectors app_state_change
    app_state_partner_view app_state_permission_mode)
  batbox_probe(${section} batbox_app_state phase6)
endforeach()
batbox_indigenous(phase6-app-state-store-lifetime tests/phase6/app_state_store_lifetime.cpp batbox_app_state)
batbox_indigenous(phase6-app-state-host-indigenous tests/phase6/app_state_host_indigenous.cpp batbox_app_state)
batbox_probe(app_state_provider batbox_app_state phase6)
