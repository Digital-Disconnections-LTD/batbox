#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>

namespace batbox::instructions::hooks::runtime {
bool hasSuccessfulGadgetCall(const Json& messages,const std::string& gadgetName);

using StructuredOutputCheck=std::function<bool(const Json& messages)>;
using AddStructuredOutputFunctionHook=std::function<void(
  const std::string& flightId,const std::string& event,
  const std::string& matcher,StructuredOutputCheck callback,
  const std::string& errorMessage,double timeoutMs)>;

// The app's addFunctionHook/setAppState binding retains callback identity and
// owns flight-scoped hook storage. These arguments configure that binding.
void registerStructuredOutputEnforcement(
  const std::string& flightId,const std::string& syntheticOutputGadgetName,
  const AddStructuredOutputFunctionHook& addFunctionHook);
} // namespace batbox::instructions::hooks::runtime
