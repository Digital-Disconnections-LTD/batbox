target_sources(batbox_interactive PRIVATE
  src/interactive/message_list_view/SearchFields.cpp
  src/interactive/message_list_view/SearchMessages.cpp
  src/interactive/message_list_view/SurfaceState.cpp
  src/interactive/message_list_view/SurfacePlan.cpp)
target_sources(batbox_interactive PRIVATE
  src/interactive/viewport/Measurements.cpp
  src/interactive/viewport/Range.cpp)
target_sources(batbox_interactive PRIVATE
  src/interactive/message_actions/MessageActionsController.cpp
  src/interactive/message_actions/MessageActionsText.cpp)
target_sources(batbox_terminal_ftxui PRIVATE
  src/interactive/transcript_navigation/NavigationSearch.cpp
  src/interactive/transcript_navigation/NavigationSeek.cpp
  src/interactive/transcript_navigation/NavigationSticky.cpp)
target_sources(batbox_terminal_ftxui PRIVATE
  src/interactive/message_list_view/Projection.cpp
  src/interactive/viewport/Controller.cpp
  src/terminal_ftxui/ElementSearch.cpp)
