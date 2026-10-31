#pragma once
#include "terminal_dom/style/Style.hpp"
#include <limits>
#include <string>

namespace batbox::terminal_dom::style::detail {
inline constexpr double NaN=std::numeric_limits<double>::quiet_NaN();
const Json* field(const Json& object,const char* key);
bool undefined(const Json& value);
bool nullish(const Json& value);
bool truthy(const Json& value);
bool number(const Json& value);
bool string(const Json& value);
double asNumber(const Json& value);
double numberOr(const Json* value,double fallback);
double parseInt10(const std::string& value);
std::string asString(const Json& value);
} // namespace batbox::terminal_dom::style::detail
