include(cmake/TerminalTextSdk.cmake)
add_library(batbox_termio src/termio/Tokenizer.cpp src/termio/Csi.cpp src/termio/Dec.cpp
  src/termio/Clear.cpp src/termio/OscLink.cpp src/termio/Output.cpp src/termio/Focus.cpp
  src/termio/OutputBinding.cpp)
target_sources(batbox_termio PRIVATE src/termio/Querier.cpp src/termio/QueryBuilders.cpp)
target_link_libraries(batbox_termio PUBLIC batbox_core batbox_fs_operations)
target_compile_options(batbox_termio PRIVATE -Wall -Wextra -Wpedantic)
include(cmake/TerminalParser.cmake)
add_library(batbox_text_width src/text_width/StringWidth.cpp src/text_width/Classification.cpp
  src/text_width/Indigenous.cpp src/text_width/Patterns.cpp src/text_width/EastAsianWidth.cpp
  src/text_width/LineWidthStash.cpp src/text_width/Measure.cpp src/text_width/Wrap.cpp
  src/text_width/AnsiSlice.cpp src/text_width/IndigenousLayout.cpp src/text_width/Tabs.cpp)
target_link_libraries(batbox_text_width PUBLIC batbox_intl batbox_glob batbox_core
  batbox_ansi_tokenize batbox_termio batbox_bun_text_width batbox_npm_wrap_ansi)
target_compile_options(batbox_text_width PRIVATE -Wall -Wextra -Wpedantic)
target_sources(batbox_termio PRIVATE src/termio/Capabilities.cpp src/termio/CapabilitiesBinding.cpp)
