#pragma once
#include "mcp/Validation.hpp"

namespace batbox::mcp {
struct ProcessResultServices {
  ResultServices result;
  ValidationServices validation;
  std::function<Json(const std::string&,const std::string&)> persistGadgetResult;
  std::function<std::int64_t()> nowMs;
  std::optional<std::string> enableLargeOutputFiles;
  std::string ideServerName="ide";
};
Json processMCPResult(const Json& result,const std::string& gadget,
  const std::string& serverName,const ProcessResultServices& services);
void bindIndigenousProcessResult(Services& gadgetServices,const ProcessResultServices& services);
} // namespace batbox::mcp
