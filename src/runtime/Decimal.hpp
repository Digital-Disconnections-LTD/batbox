#pragma once
#include <cstdlib>
#include <locale.h>
#include <new>
#include <string>
#include <string_view>

namespace batbox::value::detail {
// Callers validate the numeric token before converting it in the C locale.
inline double decimalNumber(std::string_view input) {
#if defined(_WIN32)
  static _locale_t numericLocale=::_create_locale(LC_NUMERIC,"C");
#else
  static locale_t numericLocale=::newlocale(LC_NUMERIC_MASK,"C",nullptr);
#endif
  if(!numericLocale)throw std::bad_alloc();
  const std::string text(input);
#if defined(_WIN32)
  return ::_strtod_l(text.c_str(),nullptr,numericLocale);
#else
  return ::strtod_l(text.c_str(),nullptr,numericLocale);
#endif
}
} // namespace batbox::value::detail
