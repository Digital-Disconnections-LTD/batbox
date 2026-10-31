#include "messages/RuntimeMessages.hpp"
#include "messages/Internal.hpp"

namespace batbox::messages {
using namespace internal;
Json createUserInterruptionMessage(bool gadgetUse,Services& services){
  return createUserMessage({{"content",Json::array({textBlock(
    gadgetUse?INTERRUPT_MESSAGE_FOR_GADGET_USE:INTERRUPT_MESSAGE)})}},services);
}
Json createProgressMessage(const Json& args,Services& services){
  return {{"type","progress"},{"data",args.at("data")},
    {"toolUseID",args.at("toolUseID")},{"parentToolUseID",args.at("parentToolUseID")},
    {"uuid",requireUUID(services)},{"timestamp",requireNow(services)}};
}
Json createGadgetResultStopMessage(const std::string& gadgetUseId){
  return {{"type","tool_result"},{"content",CANCEL_MESSAGE},
    {"is_error",true},{"tool_use_id",gadgetUseId}};
}
Json createSystemMessage(const std::string& content,const std::string& level,
    Services& services,std::optional<std::string> gadgetUseId,
    std::optional<bool> preventContinuation){
  Json message={{"type","system"},{"subtype","informational"},
    {"content",content},{"isMeta",false},{"timestamp",requireNow(services)},
    {"uuid",requireUUID(services)}};
  if(gadgetUseId)message["toolUseID"]=*gadgetUseId;
  message["level"]=level;
  if(preventContinuation.value_or(false))message["preventContinuation"]=true;
  return message;
}
Json createStopHookSummaryMessage(const Json& args,Services& services){
  Json message={{"type","system"},{"subtype","stop_hook_summary"}};
  for(const char* key:{"hookCount","hookInfos","hookErrors","preventedContinuation",
      "stopReason","hasOutput","level"})copyIf(message,args,key);
  message["timestamp"]=requireNow(services);message["uuid"]=requireUUID(services);
  for(const char* key:{"toolUseID","hookLabel","totalDurationMs"})copyIf(message,args,key);
  return message;
}
Json createTurnDurationMessage(double duration,const std::optional<Json>& budget,
    std::optional<std::size_t> count,Services& services){
  Json message={{"type","system"},{"subtype","turn_duration"},{"durationMs",duration}};
  if(budget){
    for(const auto& [source,target]:std::vector<std::pair<std::string,std::string>>{
        {"tokens","budgetTokens"},{"limit","budgetLimit"},{"nudges","budgetNudges"}})
      if(budget->contains(source))message[target]=budget->at(source);
  }
  if(count)message["messageCount"]=*count;
  message["timestamp"]=requireNow(services);message["uuid"]=requireUUID(services);
  message["isMeta"]=false;return message;
}
Json createGadgetUseSummaryMessage(const std::string& summary,const Json& ids,
    Services& services){
  return {{"type","tool_use_summary"},{"summary",summary},{"precedingToolUseIds",ids},
    {"uuid",requireUUID(services)},{"timestamp",requireNow(services)}};
}
} // namespace batbox::messages
