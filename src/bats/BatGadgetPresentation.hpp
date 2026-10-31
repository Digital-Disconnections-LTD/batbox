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
} // namespace batbox::bats
