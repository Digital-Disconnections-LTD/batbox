#pragma once
#include <optional>
#include <string>
#include <string_view>

namespace batbox::glob::detail {
struct ExtglobAnalysis {
  bool risky=false;
  std::optional<std::u16string> safeOutput;
};
// picomatch 4.0.4 default maxExtglobRecursion=0 branch.
ExtglobAnalysis analyzeRepeatedExtglob(std::u16string_view body);
} // namespace batbox::glob::detail
