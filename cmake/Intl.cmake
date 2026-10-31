add_library(batbox_intl src/intl/Stash.cpp src/intl/IndigenousSegments.cpp
  src/intl/IndigenousFormat.cpp src/intl/IndigenousLocale.cpp)
target_link_libraries(batbox_intl PUBLIC batbox_core ICU::i18n)
target_compile_options(batbox_intl PRIVATE -Wall -Wextra -Wpedantic)
