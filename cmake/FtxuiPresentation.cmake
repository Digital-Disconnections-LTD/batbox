add_library(batbox_interactive
  src/interactive/CommandUi.cpp src/interactive/StreamReducer.cpp
  src/interactive/PresentationState.cpp src/interactive/TranscriptRows.cpp
  src/interactive/QueryGuard.cpp src/interactive/QueryTurnStart.cpp
  src/interactive/QueryTurnFinally.cpp
  src/interactive/QueryExecutionStart.cpp src/interactive/QueryExecutionContext.cpp
  src/interactive/QueryExecutionConsume.cpp)
target_link_libraries(batbox_interactive PUBLIC batbox_flights batbox_event_loop
  batbox_app_state batbox_poetryEntries batbox_query)
target_compile_options(batbox_interactive PRIVATE -Wall -Wextra -Wpedantic)
include(cmake/MessageList.cmake)
include(cmake/InteractiveContext.cmake)

add_library(batbox_terminal_ftxui
  src/terminal_ftxui/Driver.cpp src/terminal_ftxui/PresentationLayout.cpp
  src/terminal_ftxui/PresentationView.cpp src/terminal_ftxui/StyleProjection.cpp
  src/terminal_ftxui/IndigenousStringMetrics.cpp
  src/terminal_ftxui/ScrollbackProjection.cpp)
target_link_libraries(batbox_terminal_ftxui PUBLIC batbox_interactive
  ftxui::component ftxui::dom ftxui::screen batbox_text_width batbox_intl)
target_compile_options(batbox_terminal_ftxui PRIVATE -Wall -Wextra -Wpedantic)
include(cmake/InteractiveCards.cmake)
include(cmake/MessageView.cmake)
include(cmake/MessageListView.cmake)
include(cmake/IndigenousMarkdown.cmake)

add_library(batbox_interaction_controller src/interactive/InteractionController.cpp)
target_link_libraries(batbox_interaction_controller PUBLIC
  batbox_terminal_ftxui batbox_keyboard_input)
target_compile_options(batbox_interaction_controller PRIVATE -Wall -Wextra -Wpedantic)

include(cmake/TextEditor.cmake)

# Adapter for packed terminal frames. The interactive application renders
# PresentationState directly and does not use this bridge.
add_library(batbox_ftxui_frame_projection src/terminal_ftxui/FrameProjection.cpp)
target_link_libraries(batbox_ftxui_frame_projection PUBLIC
  batbox_terminal_ftxui batbox_terminal_screen)
