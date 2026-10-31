add_library(batbox_terminal_parser src/termio/parser/Types.cpp src/termio/parser/Parser.cpp
  src/termio/parser/Grapheme.cpp src/termio/parser/CsiData.cpp src/termio/parser/CsiParse.cpp
  src/termio/parser/Sgr.cpp src/termio/parser/OscParse.cpp src/termio/parser/EscParse.cpp)
target_link_libraries(batbox_terminal_parser PUBLIC batbox_termio batbox_intl)
target_compile_options(batbox_terminal_parser PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_terminal_parser PRIVATE src/termio/parser/StyleCompare.cpp src/termio/parser/AnsiLeaf.cpp)
