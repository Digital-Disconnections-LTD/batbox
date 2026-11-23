include_guard(GLOBAL)
# One ICU discovery point for the whole project. Every component the sources
# use must be configured here: uc (ustring/uchar/unorm2/usymbol) and i18n
# (ucol_, uregex_, BreakIterator beyond ubrk, date format/dtptngen).
# Homebrew's icu4c is keg-only: its headers and libraries live under
# <brew-prefix>/opt/icu4c, which FindICU does not search unless directed.
if(APPLE)
  foreach(prefix /opt/homebrew/opt/icu4c /usr/local/opt/icu4c)
    if(EXISTS "${prefix}/include/unicode/unicode.h"
        AND EXISTS "${prefix}/lib/libicui18n.dylib")
      list(APPEND CMAKE_PREFIX_PATH "${prefix}")
    endif()
  endforeach()
endif()
find_package(ICU REQUIRED COMPONENTS uc i18n)
