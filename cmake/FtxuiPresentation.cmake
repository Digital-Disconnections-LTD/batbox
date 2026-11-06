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
  src/terminal_ftxui/TranscriptCopy.cpp
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

include(cmake/TextEditor.cmake)
