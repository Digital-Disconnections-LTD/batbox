#pragma once
#include "runtime/JsonValue.hpp"
#include <string>

namespace batbox::bats {
// Map a bat result into a gadget-result block using display names
// supplied by the caller's Batbox gadget roster.
Json mapBatGadgetResult(const Json& data,const std::string& gadgetUseId,
  const std::string& fileReadGadgetName,const std::string& bashGadgetName);
std::string batGadgetAutoClassifierInput(const Json& input);
Json batGadgetActivityDescription(const Json& input);
std::string batGadgetProgressText(const Json& progressMessages);
// The launching Bat's label carried as "launchedBy" in a sub-Bat card's
// progress entries, or empty.
std::string batLaunchedBy(const Json& progressMessages);
std::string groupedBatGadgetUseText(const Json& entries);
} // namespace batbox::bats
