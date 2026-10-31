target_sources(batbox_interactive PRIVATE
  src/interactive/message_view/MessagePlan.cpp
  src/interactive/message_view/BlockPlan.cpp
  src/interactive/message_view/RowPlan.cpp
  src/interactive/message_view/Metadata.cpp)
target_sources(batbox_terminal_ftxui PRIVATE
  src/interactive/message_view/Projection.cpp)
