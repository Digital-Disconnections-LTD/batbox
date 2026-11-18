# Terminal-text SDK: ANSI tokenize, glyph-width tables and wrapping
# (the Bun and npm wrapped-ANSI ports), truncate helpers, and the
# transcript row scanner. These were six separate targets --
# ansi_tokenize, bun_text_width, npm_wrap_ansi, text_width, truncate --
# plus Scan compiled inside batbox_core. They are one module: layered
# implementations of measuring and wrapping styled text. The vendored
# implementations keep their own directories so license provenance
# files stay beside their code.
add_library(batbox_text_sdk
  src/ansi_tokenize/AnsiCodes.cpp src/ansi_tokenize/Reduce.cpp
  src/ansi_tokenize/StyledChars.cpp src/ansi_tokenize/TokenizeParse.cpp
  src/ansi_tokenize/Tokenize.cpp src/ansi_tokenize/Fullwidth.cpp
  src/bun_text_width/WidthCore.cpp
  src/bun_text_width/Grapheme.cpp src/bun_text_width/Classification.cpp
  src/bun_text_width/Tables.cpp src/bun_text_width/WrapRows.cpp
  src/bun_text_width/WrapTrim.cpp src/bun_text_width/WrapPreserve.cpp
  src/bun_text_width/WrapCore.cpp src/bun_text_width/WrapSdk.cpp
  src/npm_wrap_ansi/Wrap.cpp src/npm_wrap_ansi/WrapRows.cpp
  src/npm_wrap_ansi/Preserve.cpp src/npm_wrap_ansi/Dependencies.cpp
  src/npm_wrap_ansi/Data.cpp
  src/text_width/StringWidth.cpp src/text_width/Classification.cpp
  src/text_width/Indigenous.cpp src/text_width/Patterns.cpp
  src/text_width/EastAsianWidth.cpp
  src/text_width/AnsiSlice.cpp src/text_width/IndigenousLayout.cpp
  src/format/Truncate.cpp
  src/format/TruncateExtra.cpp
  src/terminal_search/Scan.cpp
)
target_link_libraries(batbox_text_sdk PUBLIC batbox_intl batbox_glob
  batbox_core batbox_termio)
target_compile_options(batbox_text_sdk PRIVATE -Wall -Wextra -Wpedantic)
# Scan and the width helpers were reachable through batbox_core; keep
# consumers resolving through the core aggregate.
target_link_libraries(batbox_core PUBLIC batbox_text_sdk)
