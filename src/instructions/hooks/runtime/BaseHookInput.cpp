#include "instructions/hooks/runtime/BaseHookInput.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
Json BaseHookInput::toJson()const{
  Json result={{"session_id",flightId},{"transcript_path",transcriptPath},
    {"cwd",cwd}};
  if(permissionMode)result["permission_mode"]=*permissionMode;
  if(batId)result["agent_id"]=*batId;
  if(batType)result["agent_type"]=*batType;
  return result;
}
std::vector<std::string> BaseHookInput::ownKeys()const{
  return {"session_id","transcript_path","cwd","permission_mode",
    "agent_id","agent_type"};
}
BaseHookInput createBaseHookInput(const BaseHookInputServices& services,
  std::optional<std::string> permissionMode,
  std::optional<std::string> flightId,
  std::optional<HookBatInfo> batInfo){
  if(!services.getFlightId||!services.getMainThreadBatType||
    !services.getTranscriptPathForFlight||!services.getCwd)
    throw std::invalid_argument("Base hook input services required");
  const auto resolvedFlightId=flightId?*flightId:services.getFlightId();
  const auto resolvedBatType=batInfo&&batInfo->batType
    ?batInfo->batType:services.getMainThreadBatType();
  return {resolvedFlightId,
    services.getTranscriptPathForFlight(resolvedFlightId),
    services.getCwd(),permissionMode,
    batInfo?batInfo->batId:std::nullopt,resolvedBatType};
}
} // namespace batbox::instructions::hooks::runtime
