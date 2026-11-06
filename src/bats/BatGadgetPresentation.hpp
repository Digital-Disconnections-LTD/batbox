#pragma once
#include "runtime/JsonValue.hpp"
#include <string>

namespace batbox::bats {
// The Gadget a caller blocks on to wait for a background Bat.
inline constexpr const char* missionOutputGadgetName="TaskOutput";
// Map a bat result into a gadget-result block.
Json mapBatGadgetResult(const Json& data,const std::string& gadgetUseId);
std::string batGadgetAutoClassifierInput(const Json& input);
Json batGadgetActivityDescription(const Json& input);
std::string batGadgetProgressText(const Json& progressMessages);
// The launching Bat's label carried as "launchedBy" in a sub-Bat card's
// progress entries, or empty.
std::string batLaunchedBy(const Json& progressMessages);
std::string groupedBatGadgetUseText(const Json& entries);
} // namespace batbox::bats
