#include "instructions/hooks/runtime/ProcessHookJsonInternal.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
void optionalAttachment(Json& attachment,std::vector<std::string>& undefined,
  const char* key,const std::optional<std::string>& value){
  if(value)attachment[key]=*value;else undefined.push_back(key);
}
void optionalAttachment(Json& attachment,std::vector<std::string>& undefined,
  const char* key,const std::optional<double>& value){
  if(value)attachment[key]=*value;else undefined.push_back(key);
}
}
ProcessHookJsonResult processHookJSONOutput(const ProcessHookJsonInput& input,
  const ProcessHookJsonServices& services){
  if(!services.createAttachmentMessage)
    throw std::invalid_argument("Hook attachment factory required");
  ProcessHookJsonResult result;
  detail::processCommon(input,result);
  detail::processSpecific(input,result);
  Json attachment;
  std::vector<std::string> ownUndefined;
  if(result.fields.contains("blockingError")){
    attachment={{"type","hook_blocking_error"},{"hookName",input.hookName},
      {"toolUseID",input.gadgetUseID},{"hookEvent",input.hookEvent},
      {"blockingError",result.fields.at("blockingError")}};
  }else{
    attachment={{"type","hook_success"},{"hookName",input.hookName},
      {"toolUseID",input.gadgetUseID},{"hookEvent",input.hookEvent},
      {"content",""}};
    optionalAttachment(attachment,ownUndefined,"stdout",input.stdoutText);
    optionalAttachment(attachment,ownUndefined,"stderr",input.stderrText);
    optionalAttachment(attachment,ownUndefined,"exitCode",input.exitCode);
    attachment["command"]=input.command;
    optionalAttachment(attachment,ownUndefined,"durationMs",input.durationMs);
  }
  result.fields["message"]=services.createAttachmentMessage(attachment,
    ownUndefined);
  return result;
}
} // namespace batbox::instructions::hooks::runtime
