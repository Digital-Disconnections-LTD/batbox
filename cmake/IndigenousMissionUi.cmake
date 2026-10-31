# Include after IndigenousCommands and IndigenousApplicationBackend targets exist.
target_sources(batbox_application_commands PRIVATE
  src/application_mission_ui/MissionDialog.cpp
  src/application_mission_ui/BatMissionBridge.cpp)
target_link_libraries(batbox_application_commands PUBLIC
  batbox_application_shell batbox_app_state)
