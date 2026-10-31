#pragma once
#include <string>
namespace batbox::shell {
std::string formatDuration(double milliseconds,bool hideTrailingZeros=false,
  bool mostSignificantOnly=false);
}
