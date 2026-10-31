add_library(batbox_structured_io
  src/cli/structured/ControlResponses.cpp
  src/cli/structured/ControlMessageCompat.cpp
  src/cli/structured/ProcessLine.cpp
  src/cli/structured/IndigenousProcessLine.cpp
  src/cli/structured/RequestProducer.cpp
  src/cli/structured/InputReader.cpp
  src/cli/structured/ControlAdapters.cpp
  src/cli/structured/OutputWrite.cpp
)
target_link_libraries(batbox_structured_io PUBLIC batbox_headless_cli batbox_event_loop)
target_compile_options(batbox_structured_io PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_terminal_leaves
  src/terminal_runtime_roster/ColorSdk.cpp
  src/terminal_runtime_roster/HyperlinkSdk.cpp
  src/terminal_runtime_roster/MeasureElement.cpp
  src/terminal_runtime_roster/SupportsHyperlinks.cpp
)
target_link_libraries(batbox_terminal_leaves PUBLIC batbox_terminal_runtime)
target_compile_options(batbox_terminal_leaves PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_query_engine_state
  src/query_engine/ConversationState.cpp
)
target_link_libraries(batbox_query_engine_state PUBLIC batbox_core)
target_compile_options(batbox_query_engine_state PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_terminal_runtime PRIVATE
  src/terminal_runtime/TerminalRenderCycle.cpp
  src/terminal_runtime/TerminalOwnerRoot.cpp
  src/terminal_runtime/SearchHighlight.cpp)
