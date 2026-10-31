#pragma once
#include "instructions/hooks/runtime/ProcessHookJson.hpp"

namespace batbox::instructions::hooks::runtime::detail {
void processCommon(const ProcessHookJsonInput& input,
  ProcessHookJsonResult& result);
void processSpecific(const ProcessHookJsonInput& input,
  ProcessHookJsonResult& result);
void assignOptional(ProcessHookJsonResult& result,const std::string& name,
  const Json& object,const std::string& key);
void block(ProcessHookJsonResult& result,const std::string& reason,
  const std::string& command);
std::string stringValue(const Json& value);
std::string fieldString(const Json& object,const std::string& key);
} // namespace batbox::instructions::hooks::runtime::detail
