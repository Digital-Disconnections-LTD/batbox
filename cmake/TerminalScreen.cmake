add_library(batbox_terminal_screen
  src/terminal_screen/CellRead.cpp
  src/terminal_screen/CellWrite.cpp
  src/terminal_screen/DiffRows.cpp
  src/terminal_screen/Geometry.cpp
  src/terminal_screen/HyperlinkStyles.cpp
  src/terminal_screen/PackedCells.cpp
  src/terminal_screen/RegionBlit.cpp
  src/terminal_screen/RegionClear.cpp
  src/terminal_screen/ScreenCore.cpp
  src/terminal_screen/ScreenDiff.cpp
  src/terminal_screen/SelectionMarks.cpp
  src/terminal_screen/ShiftRows.cpp
  src/terminal_screen/StringPools.cpp
  src/terminal_screen/StylePool.cpp
)
target_link_libraries(batbox_terminal_screen PUBLIC batbox_ansi_tokenize batbox_core)
target_compile_options(batbox_terminal_screen PRIVATE -Wall -Wextra -Wpedantic)
