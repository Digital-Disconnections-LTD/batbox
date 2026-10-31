#pragma once
#include "runtime/JsonValue.hpp"
#include <optional>
#include <string>
#include <vector>

namespace batbox::application_memory {
double readLastConsolidatedAt(const std::string& directory);
std::optional<double> tryAcquireConsolidationLock(const std::string& directory,
  double nowMillis);
void rollbackConsolidationLock(const std::string& directory,double priorMtime);
std::vector<std::string> flightsTouchedSince(const std::string& directory,
  double since,const std::string& currentFlight);
std::string buildConsolidationPoetry(const std::string& memoryRoot,
  const std::string& transcriptDir,const std::string& extra);
} // namespace batbox::application_memory
