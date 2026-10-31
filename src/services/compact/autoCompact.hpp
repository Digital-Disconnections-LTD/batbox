#pragma once
#include "utils/envUtils.hpp"
namespace batbox {
struct AutoCompactDependencies {
  double contextWindow;
  double maxOutputTokens;
  bool autoCompactEnabled = true;
  Environment env;
};
inline constexpr double MAX_OUTPUT_TOKENS_FOR_SUMMARY = 20000;
inline constexpr double AUTOCOMPACT_BUFFER_TOKENS = 13000;
inline constexpr double WARNING_THRESHOLD_BUFFER_TOKENS = 20000;
inline constexpr double ERROR_THRESHOLD_BUFFER_TOKENS = 20000;
inline constexpr double MANUAL_COMPACT_BUFFER_TOKENS = 3000;
double getEffectiveContextWindowSize(const AutoCompactDependencies& deps);
double getAutoCompactThreshold(const AutoCompactDependencies& deps);
bool isAutoCompactEnabled(const AutoCompactDependencies& deps);
Json calculateTokenWarningState(double tokenUsage, const AutoCompactDependencies& deps);
} // namespace batbox
