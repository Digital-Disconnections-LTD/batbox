add_library(batbox_terminal_dom
  src/terminal_dom/DomCreate.cpp
  src/terminal_dom/DomDirty.cpp
  src/terminal_dom/DomMeasure.cpp
  src/terminal_dom/DomIndigenousText.cpp
  src/terminal_dom/DomProps.cpp
  src/terminal_dom/DomTree.cpp
  src/terminal_dom/Focus.cpp
  src/terminal_dom/FocusTree.cpp
  src/terminal_dom/IndigenousYogaDom.cpp
  src/terminal_dom/Node.cpp
  src/terminal_dom/NodeStash.cpp
  src/terminal_dom/YogaLayoutCore.cpp
  src/terminal_dom/YogaLayoutStyle.cpp
  src/terminal_dom/YogaMaps.cpp
)
target_link_libraries(batbox_terminal_dom PUBLIC batbox_terminal_events batbox_indigenous_yoga batbox_text_width batbox_terminal_screen)
target_compile_options(batbox_terminal_dom PRIVATE -Wall -Wextra -Wpedantic)
