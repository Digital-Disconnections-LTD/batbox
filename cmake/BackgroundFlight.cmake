add_library(batbox_background_flight
  src/interactive/background/BackgroundQuery.cpp
  src/interactive/background/NotificationAttachments.cpp
  src/interactive/background/FlightBackgrounding.cpp
  src/interactive/background/FlightMissionState.cpp
  src/interactive/background/FlightMissionRun.cpp)
target_link_libraries(batbox_background_flight PUBLIC batbox_poetry_submit
  batbox_bat_context batbox_interactive batbox_poetryEntries batbox_core
  batbox_app_state batbox_event_loop)
target_compile_options(batbox_background_flight PRIVATE -Wall -Wextra -Wpedantic)
