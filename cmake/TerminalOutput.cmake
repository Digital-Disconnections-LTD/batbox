add_library(batbox_terminal_output
  src/terminal_output/Bidi.cpp
  src/terminal_output/BidiEmbedding.cpp
  src/terminal_output/BidiExplicit.cpp
  src/terminal_output/BidiImplicit.cpp
  src/terminal_output/BidiNeutral.cpp
  src/terminal_output/BidiRuns.cpp
  src/terminal_output/BidiTypes.cpp
  src/terminal_output/BidiWeak.cpp
  src/terminal_output/Clip.cpp
  src/terminal_output/LineClustering.cpp
  src/terminal_output/LineIndigenous.cpp
  src/terminal_output/LineWrite.cpp
  src/terminal_output/IndigenousBidi.cpp
  src/terminal_output/OutputGet.cpp
  src/terminal_output/OutputIndigenous.cpp
  src/terminal_output/OutputQueue.cpp
  src/terminal_output/OutputWrite.cpp
)
target_link_libraries(batbox_terminal_output PUBLIC batbox_terminal_screen batbox_ansi_tokenize batbox_intl batbox_text_width batbox_core)
target_compile_options(batbox_terminal_output PRIVATE -Wall -Wextra -Wpedantic)
