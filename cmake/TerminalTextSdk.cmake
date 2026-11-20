# Terminal-text SDK: ANSI tokenize, glyph-width tables and wrapping
# (the Bun and npm wrapped-ANSI ports), truncate helpers, and the
# transcript row scanner. These were six separate targets --
# ansi_tokenize, bun_text_width, npm_wrap_ansi, text_width, truncate --
# plus Scan compiled inside batbox_core. They are one module: layered
# implementations of measuring and wrapping styled text. The vendored
# implementations keep their own directories so license provenance
# files stay beside their code.
add_library(batbox_text_sdk
  src/text_sdk/ansi_tokenize/AnsiCodes.cpp src/text_sdk/ansi_tokenize/Reduce.cpp
  src/text_sdk/ansi_tokenize/StyledChars.cpp src/text_sdk/ansi_tokenize/TokenizeParse.cpp
  src/text_sdk/ansi_tokenize/Tokenize.cpp src/text_sdk/ansi_tokenize/Fullwidth.cpp
  src/text_sdk/bun_text_width/WidthCore.cpp
  src/text_sdk/bun_text_width/Grapheme.cpp src/text_sdk/bun_text_width/Classification.cpp
  src/text_sdk/bun_text_width/Tables.cpp src/text_sdk/bun_text_width/WrapRows.cpp
  src/text_sdk/bun_text_width/WrapTrim.cpp src/text_sdk/bun_text_width/WrapPreserve.cpp
  src/text_sdk/bun_text_width/WrapCore.cpp src/text_sdk/bun_text_width/WrapSdk.cpp
  src/text_sdk/npm_wrap_ansi/Wrap.cpp src/text_sdk/npm_wrap_ansi/WrapRows.cpp
  src/text_sdk/npm_wrap_ansi/Preserve.cpp src/text_sdk/npm_wrap_ansi/Dependencies.cpp
  src/text_sdk/npm_wrap_ansi/Data.cpp
  src/text_sdk/width/StringWidth.cpp src/text_sdk/width/Classification.cpp
  src/text_sdk/width/Indigenous.cpp src/text_sdk/width/Patterns.cpp
  src/text_sdk/width/EastAsianWidth.cpp
  src/text_sdk/width/AnsiSlice.cpp src/text_sdk/width/IndigenousLayout.cpp
  src/text_sdk/truncate/Truncate.cpp
  src/text_sdk/truncate/TruncateExtra.cpp
  src/text_sdk/scan/Scan.cpp
)
target_link_libraries(batbox_text_sdk PUBLIC batbox_intl batbox_glob
  batbox_core batbox_termio)
target_compile_options(batbox_text_sdk PRIVATE -Wall -Wextra -Wpedantic)
# Scan and the width helpers were reachable through batbox_core; keep
# consumers resolving through the core aggregate.
target_link_libraries(batbox_core PUBLIC batbox_text_sdk)
