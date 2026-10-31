add_library(batbox_app_state src/app_state/DefaultState.cpp src/app_state/Selectors.cpp
  src/app_state/ExternalMetadata.cpp src/app_state/OnChange.cpp src/app_state/Binding.cpp
  src/app_state/PartnerView.cpp src/app_state/PermissionModeBridge.cpp src/app_state/Host.cpp)
target_link_libraries(batbox_app_state PUBLIC batbox_sdk_events batbox_event_loop)
target_compile_options(batbox_app_state PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_app_state PRIVATE src/app_state/provider/PermissionHelpers.cpp
  src/app_state/provider/Effects.cpp src/app_state/provider/TypedSettings.cpp)
target_link_libraries(batbox_app_state PUBLIC batbox_settings)
