#pragma once
#include "mcp/Mcp.hpp"

namespace batbox::mcp {
GadgetCallResult callMCPGadgetWithUrlElicitationRetry(
  const Connection& connection,const std::string& gadget,const Json& args,
  std::stop_token signal,const std::function<void(const Json&)>& onProgress,
  const UrlElicitationServices& services);
bool isMcpFlightExpiredError(int status,const std::string& message);
} // namespace batbox::mcp
