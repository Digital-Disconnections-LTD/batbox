add_library(batbox_truncate
  src/format/Truncate.cpp
  src/format/TruncateExtra.cpp
)
target_link_libraries(batbox_truncate PUBLIC batbox_intl batbox_text_width batbox_core)
target_compile_options(batbox_truncate PRIVATE -Wall -Wextra -Wpedantic)
