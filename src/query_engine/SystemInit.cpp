#include "query_engine/SystemInit.hpp"
#include <stdexcept>
namespace batbox::query_engine {
namespace {
template<class F>const F& need(const F& fn,const char* name){
  if(!fn)throw std::invalid_argument(std::string("System init import required: ")+name);
  return fn;
}
Json commandNames(const Json& commands){
  Json names=Json::array();
  for(const auto& command:commands){
    const auto& invocable=value::property(command,"userInvocable");
    if(!invocable.is_boolean()||invocable.get<bool>())names.push_back(command.at("name"));
  }
  return names;
}
}
std::string sdkCompatGadgetName(const std::string& name){return name=="Agent"?"Task":name;}
Json buildSystemInitMessage(const SystemInitInputs& inputs,const SystemInitServices& services){
  const auto settings=need(services.getSettings,"getSettings")();
  const auto& style=value::property(settings,"outputStyle");
  const Json outputStyle=style.is_null()||style.is_discarded()?Json(services.defaultOutputStyle):style;
  Json message=Json::object();
  message["type"]="system";message["subtype"]="init";
  message["cwd"]=need(services.getCwd,"getCwd")();
  message["session_id"]=need(services.getFlightId,"getSessionId")();
  message["tools"]=Json::array();
  for(const auto& gadget:inputs.gadgets)message["tools"].push_back(sdkCompatGadgetName(gadget.at("name").get<std::string>()));
  message["mcp_servers"]=Json::array();
  for(const auto& client:inputs.mcpClients)message["mcp_servers"].push_back(Json{{"name",client.at("name")},{"status",client.at("type")}});
  message["model"]=inputs.model;message["permissionMode"]=inputs.permissionMode;
  message["slash_commands"]=commandNames(inputs.commands);
  message["apiKeySource"]=need(services.getApiKeySource,"getApiKeySource")();
  message["betas"]=need(services.getSdkBetas,"getSdkBetas")();
  message["batbox_code_version"]=services.version;
  message["output_style"]=outputStyle;
  message["agents"]=Json::array();
  for(const auto& bat:inputs.bats)message["agents"].push_back(bat.at("agentType"));
  message["skills"]=commandNames(inputs.skills);
  message["plugins"]=Json::array();
  for(const auto& plugin:inputs.plugins)message["plugins"].push_back(Json{{"name",plugin.at("name")},{"path",plugin.at("path")},{"source",plugin.at("source")}});
  message["uuid"]=need(services.randomUUID,"randomUUID")();
  if(need(services.feature,"feature")("UDS_INBOX"))
    message["messaging_socket_path"]=need(services.getMessagingSocketPath,"getMessagingSocketPath")();
  const auto fast=need(services.getFastModeState,"getFastModeState")(inputs.model,inputs.fastMode);
  if(fast)message["fast_mode_state"]=*fast;
  return message;
}
}
