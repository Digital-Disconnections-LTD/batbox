add_library(batbox_bun_text_width
  src/bun_text_width/Sdk.cpp src/bun_text_width/WidthCore.cpp
  src/bun_text_width/Grapheme.cpp src/bun_text_width/Classification.cpp
  src/bun_text_width/Tables.cpp src/bun_text_width/WrapRows.cpp
  src/bun_text_width/WrapTrim.cpp src/bun_text_width/WrapPreserve.cpp
  src/bun_text_width/WrapCore.cpp src/bun_text_width/WrapSdk.cpp)
target_link_libraries(batbox_bun_text_width PUBLIC batbox_core)
target_compile_options(batbox_bun_text_width PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_ansi_tokenize
  src/ansi_tokenize/AnsiCodes.cpp src/ansi_tokenize/Reduce.cpp
  src/ansi_tokenize/StyledChars.cpp src/ansi_tokenize/TokenizeParse.cpp
  src/ansi_tokenize/Tokenize.cpp src/ansi_tokenize/Fullwidth.cpp)
target_link_libraries(batbox_ansi_tokenize PUBLIC batbox_intl batbox_core)
target_compile_options(batbox_ansi_tokenize PRIVATE -Wall -Wextra -Wpedantic)
add_library(batbox_npm_wrap_ansi src/npm_wrap_ansi/Wrap.cpp src/npm_wrap_ansi/WrapRows.cpp
  src/npm_wrap_ansi/Preserve.cpp src/npm_wrap_ansi/Dependencies.cpp src/npm_wrap_ansi/Data.cpp)
target_link_libraries(batbox_npm_wrap_ansi PUBLIC batbox_glob batbox_intl batbox_core)
target_compile_options(batbox_npm_wrap_ansi PRIVATE -Wall -Wextra -Wpedantic)
